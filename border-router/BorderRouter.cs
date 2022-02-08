/************************************************************************************************//**
 * @file		BorderRouter.cs
 *
 * @copyright	Copyright 2022 Kurt Hildebrand.
 * @license		Licensed under the Apache License, Version 2.0 (the "License"); you may not use this
 *				file except in compliance with the License. You may obtain a copy of the License at
 *
 *				http://www.apache.org/licenses/LICENSE-2.0
 *
 *				Unless required by applicable law or agreed to in writing, software distributed under
 *				the License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF
 *				ANY KIND, either express or implied. See the License for the specific language
 *				governing permissions and limitations under the License.
 *
 ***************************************************************************************************/
using CoAP;
using Hyperspace.Firmware;
using Hyperspace.Hubs;
using Hyperspace.Models;
using Microsoft.AspNetCore.Http;
using Microsoft.AspNetCore.Mvc;
using Microsoft.AspNetCore.SignalR;
using Microsoft.EntityFrameworkCore;
using Microsoft.Extensions.Hosting;
using Microsoft.Extensions.Logging;
using Microsoft.Extensions.Options;
using NetTopologySuite.Geometries;
using NetTopologySuite.IO;
using Npgsql;
using NpgsqlTypes;
using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.IO;
using System.Linq;
using System.Net;
using System.Net.NetworkInformation;
using System.Net.Sockets;
using System.Text;
using System.Text.Json;
using System.Text.Json.Serialization;
using System.Text.RegularExpressions;
using System.Threading;
using System.Threading.Tasks;


namespace Hyperspace
{
	public class BorderRouter: BackgroundService
	{
		private HyperTun                            hypertun { get; set; }
		private ILogger<BorderRouter>               logger   { get; set; }
		private IHubContext<DbStreamHub, IDbStream> hub      { get; set; }
		private DatabaseOptions                     options  { get; set; }
		private static int ReportsPort = 2200;
		private static int MulticastPort = 2201;

		public BorderRouter(
			ILogger<BorderRouter>               logger,
			ILogger<HyperTun>                   hypertunLogger,
			IHubContext<DbStreamHub, IDbStream> hub,
			IOptions<DatabaseOptions>           options,
			FirmwareService fwService)
		{
			this.logger   = logger;
			this.hub      = hub;
			this.options  = options.Value;
			this.hypertun = new HyperTun(hypertunLogger, this.options.HyperspaceConnectionString);
		}

		public void SetupApi(Microsoft.AspNetCore.Builder.WebApplication app)
		{
			app.MapGet("api/devices", async (HyperspaceDbContext db) =>
				await db.Devices.ToListAsync());

			app.MapGet("api/devices/{ip}", async (string ip, HyperspaceDbContext db) =>
				await db.Devices.FindAsync(ip)
					is HyperspaceDevice device
						? Results.Ok(device)
						: Results.NotFound());

			app.MapGet("api/info", () =>
				Results.Ok(new { name = "Test Network"}));

			app.MapGet("api/reports", async (HyperspaceDbContext db) =>
				await db.Reports
					.FromSqlRaw(@"SELECT DISTINCT ON (ip) * FROM reports ORDER BY ip, updated_at DESC;")
					.ToListAsync());
		}

		protected override async Task ExecuteAsync(CancellationToken token)
		{
			hypertun.Setup();

			await Task.WhenAll(
				hypertun.Run(token),
				ReportsListener(token),
				DbPollImageInfo(token),
				DbPollCoapWellKnown(token),
				DbWatchDataChanged(token)
			);
		}

		#region Reports Listener
		private async Task ReportsListener(CancellationToken token)
		{
			/* Setup connection to the database */
			await using var db = new NpgsqlConnection(options.HyperspaceConnectionString);
			await db.OpenAsync();

			/* Setup UDP listener */
			var udp = new UdpClient(ReportsPort, AddressFamily.InterNetworkV6);

			logger.LogInformation("Listening for reports on port " + ReportsPort);

			while(!token.IsCancellationRequested)
			{
				var    res     = await udp.ReceiveAsync();
				var    utcnow  = DateTime.UtcNow;
				var    ep_addr = res.RemoteEndPoint.Address;
				var    ep_port = res.RemoteEndPoint.Port;

				string str     = Encoding.UTF8.GetString(res.Buffer);
				var    json    = JsonSerializer.Deserialize<Dictionary<string, JsonElement>>(str);

				/* Todo: there seems to be some bug in .NET 6 regarding GetSingle() and NaNs */
				var    loc     = json["loc"];
				float  x       = Convert.ToSingle(loc[0].ToString());
				float  y       = Convert.ToSingle(loc[1].ToString());
				float  z       = Convert.ToSingle(loc[2].ToString());

				json.Remove("loc");

				string reserialized = JsonSerializer.Serialize(json);

				await using(var cmd = new NpgsqlCommand(
					"INSERT INTO reports (ip, updated_at, loc, report) " +
					"VALUES(@ip, @updated_at, @loc, @report)", db))
				{
					cmd.Parameters.AddWithValue("ip",         ep_addr);
					cmd.Parameters.AddWithValue("updated_at", utcnow);
					cmd.Parameters.AddWithValue("loc",        new Point(x, y, z));
					cmd.Parameters.AddWithValue("report", NpgsqlDbType.Jsonb, reserialized);
					await cmd.ExecuteNonQueryAsync();
				}
			}

			logger.LogError("Reports listener cancellation requested");
		}
		#endregion
		#region Database Listener
		private async Task DbPollImageInfo(CancellationToken token)
		{
			while(!token.IsCancellationRequested)
			{
				await Task.Delay(30000);
				await PollAndUpdateDb("image_0", "firmware", token);
			}
		}

		private async Task DbPollCoapWellKnown(CancellationToken token)
		{
			while(!token.IsCancellationRequested)
			{
				await Task.Delay(30000);
				await PollAndUpdateDb("coap_well_known", ".well-known/core", token);
			}
		}

		private async Task PollAndUpdateDb(string column_name, string coap_ep, CancellationToken token)
		{
			var requests = new List<Task<(string ip, string response)>>();
			await using var conn = new NpgsqlConnection(options.HyperspaceConnectionString);
			await conn.OpenAsync();
			await using(var cmd = new NpgsqlCommand(
				$"SELECT * FROM devices WHERE {column_name} is null", conn))
			await using(var reader = await cmd.ExecuteReaderAsync(token))
			{
				while(await reader.ReadAsync())
				{
					string ip = reader["ip"].ToString();

					if(ip == "::")
					{
						continue;
					}

					logger.LogInformation($"Reading {column_name} from {ip}");
					requests.Add(CoapGetAsync(ip, coap_ep));
				}
			}

			/* Update the devices table */
			/* ref: https://docs.microsoft.com/en-us/dotnet/csharp/programming-guide/concepts/async/start-multiple-async-tasks-and-process-them-as-they-complete */
			while(requests.Count > 0)
			{
				var request = await Task.WhenAny(requests);
				requests.Remove(request);
				(string ip, string fw) = request.Result;
				if(fw != string.Empty)
				{
					/* Todo: test */
					await using(var cmd = new NpgsqlCommand(
						$"UPDATE devices SET {column_name} = @fw WHERE ip = @ip", conn))
					{
						cmd.Parameters.AddWithValue("fw", fw);
						cmd.Parameters.AddWithValue("ip", IPAddress.Parse(ip));
						await cmd.ExecuteNonQueryAsync();
					}
				}
			}

			await conn.CloseAsync();
		}

		private Task<(string ip, string response)> CoapGetAsync(string ip, string ep)
		{
			var tcs     = new TaskCompletionSource<(string ip, string response)>();
			var request = new Request(Method.GET);
			request.URI = new Uri($"coap://[{ip}]/{ep}");

			request.Respond  += (s, e) =>
			{
				tcs.TrySetResult((ip, e.Response.ResponseText));
			};

			request.TimedOut += (s, e) =>
			{
				tcs.TrySetResult((ip, string.Empty));
			};

			request.Send();

			return tcs.Task;
		}

		private async Task DbWatchDataChanged(CancellationToken token)
		{
			await using var conn = new NpgsqlConnection(options.HyperspaceConnectionString);
			await conn.OpenAsync();
			conn.Notification += HandleNotifications;

			await using(var cmd = new NpgsqlCommand("LISTEN data_changed", conn))
			{
				await cmd.ExecuteNonQueryAsync();
			}

			while(!token.IsCancellationRequested)
			{
				await conn.WaitAsync();
			}

			logger.LogError("Cancellation Requested");

			conn.Notification -= HandleNotifications;
		}

		private async void HandleNotifications(object sender, NpgsqlNotificationEventArgs e)
		{
			/* Example data: "{\"table\" : \"coordinates\", \"action\" : \"UPDATE\", \"data\" : {\"ip\":\"fd00::1234\",\"updated_at\":\"2021-06-16T22:25:24.520495+00:00\",\"r\":2.5,\"t\":-1.25,\"seq\":null}}"
			 *
			 * 	{
			 * 		\"table\" : \"coordinates\",
			 * 		\"action\" : \"UPDATE\",
			 * 		\"data\" : {
			 * 			\"ip\":\"fd00::1234\",
			 * 			\"updated_at\":\"2021-06-16T22:25:24.520495+00:00\",
			 * 			\"r\":2.5,
			 * 			\"t\":-1.25,
			 * 			\"seq\":null
			 * 		}
			 * 	}
			 */
			if(e.Channel != "data_changed")
			{
				return;
			}

			// logger.LogInformation("Received notification: " + e.Payload);

			using (var doc = JsonDocument.Parse(e.Payload))
			{
				JsonElement root  = doc.RootElement;
				JsonElement table = root.GetProperty("table");

				if(table.ToString() == "devices")
				{
					await hub.Clients.All.UpdateDevices(e.Payload);
				}
				else if(table.ToString() == "reports")
				{
					await UpdateReportsWorkaround(e.Payload);

					// await hub.Clients.All.UpdateReports(e.Payload);
				}
			}
		}

		private async Task UpdateReportsWorkaround(string s)
		{
			/* There is weird behavior in postgresql-9.6 where row_to_json returns the raw EWKB text
			 * for geometry columns. I want the GeoJSON representation of this column but I don't
			 * want a custom class to serialize into, modify, and deserialize just to get the correct
			 * JSON. Also, there seems to be no good way to modify things using JsonDocument and
			 * JsonElements. Therefore, I'm resorting to this mess to change the text into something
			 * that wont' crash the javascript. Hopefully Raspberry Pi gets postgresql-13 which seems
			 * to have the correct behavior. Then this will not be needed. */
			Regex regex = new Regex("(\"[0-9A-F]*\")",
				RegexOptions.Compiled | RegexOptions.IgnoreCase);

			var match  = regex.Match(s);
			var bytes  = WKBReader.HexToBytes(match.Value.Substring(1, match.Length-2));
			var reader = new WKBReader();
			var point  = reader.Read(bytes);

			var modstr = s
				.Remove(match.Index, match.Length)
				.Insert(match.Index,
					String.Format("{{\"type\": \"Point\", \"coordinates\": [{0},{1},{2}]}}",
						point.Coordinate.X, point.Coordinate.Y, point.Coordinate.Z));

			await hub.Clients.All.UpdateReports(modstr);
		}
		#endregion

		private async Task PingTunspi()
		{
			var interfaces = NetworkInterface.GetAllNetworkInterfaces();
			var tunspi = interfaces.FirstOrDefault(i => i.Name == "tun0");

			if(tunspi is null)
			{
				logger.LogError("Could not find tunspi");
				return;
			}

			var index = tunspi.GetIPProperties()?.GetIPv4Properties()?.Index;

			if(!index.HasValue)
			{
				logger.LogError("Could not find tunspi index");
				return;
			}

			var dest  = IPAddress.Parse(String.Format("FF22::2%{0}", tunspi.Name));
			var ping  = new Ping();
			var opts  = new PingOptions(1, true);
			var reply = await ping.SendPingAsync(dest, 30000, null, opts);


		}
	}
}
