/************************************************************************************************//**
 * @file		FirmwareUpdate.cs
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
using CoAP.Net;
using Hyperspace.Firmware;
using Hyperspace.Hubs;
using Hyperspace.Models;
using Microsoft.AspNetCore.Http;
using Microsoft.AspNetCore.Mvc;
using Microsoft.AspNetCore.SignalR;
using Microsoft.Extensions.DependencyInjection;
using Microsoft.Extensions.Hosting;
using Microsoft.Extensions.Logging;
using Microsoft.Extensions.Options;
using System;
using System.Collections.Concurrent;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Text.Json.Serialization;
using System.Threading;
using System.Threading.Tasks;


namespace Hyperspace
{
	[ApiController]
	[Route("api/firmware")]
	public class FirmwareController : ControllerBase
	{
		private ILogger<FirmwareController> logger { get; set; }
		private IHubContext<FirmwareUpdateHub, IFirmwareUpdateHub> hub { get; set; }
		private FirmwareService fwService { get; set; }
		private string[] permitted_extensions = { ".bin" };
		private string   fw_dir = "firmware/";

		public FirmwareController(
			ILogger<FirmwareController> logger,
			IHubContext<FirmwareUpdateHub, IFirmwareUpdateHub> hub,
			FirmwareService fwService)
		{
			this.logger    = logger;
			this.hub       = hub;
			this.fwService = fwService;

			if(!Directory.Exists(fw_dir))
			{
				Directory.CreateDirectory(fw_dir);
			}
		}

		[HttpGet]
		public ActionResult<List<string>> LatestFirmware()
		{
			// List<string> files = new List<string>
			// {
			// 	"kh,mesh-root,1.0",
			// 	"kh,mesh-root,1.1",
			// 	"kh,mesh-root,2.0",
			// 	"kh,mesh-root,3.0",
			// 	"kh,mesh-root,3.2",
			// 	"kh,mesh-beacon,1.0",
			// 	"kh,mesh-beacon,1.10",
			// 	"kh,mesh-beacon,1.2",
			// 	"kh,mesh-beacon,1.1.2",
			// 	"kh,mesh-nonbeacon,1.0",
			// 	"kh,mesh-nonbeacon,2.0",
			// 	"kh,mesh-nonbeacon,3.0",
			// 	"kh,mesh-nonbeacon,4.0",
			// 	"kh,mesh-nonbeacon,5.0",
			// 	"kh,mesh-nonbeacon,6.0",
			// };

			/* Todo: migrate to FirmwareService.cs */
			// var files  = Directory.GetFiles(fw_dir);
			// var latest = files
			// 	.Select(s => new FirmwareString(s))
			// 	.OrderBy(fw => fw)
			// 	.GroupBy(fw => $"{fw.Manuf},{fw.Board}")
			// 	.Select(group => group.Last()).ToList();

			// return latest;

			return fwService.LatestFirmware();
		}

		[HttpPost]
		public async Task<IActionResult> Upload(IList<IFormFile> files)
		{
			long size = files.Sum(f => f.Length);

			foreach(var file in files)
			{
				/* Verify extension */
				var ext = Path.GetExtension(file.FileName).ToLowerInvariant();

				if(string.IsNullOrEmpty(ext) || !permitted_extensions.Contains(ext))
				{
					logger.LogWarning("Unpermitted extension {0}", ext);
					continue;
				}

				/* Todo: verify image */
				McuBoot mcuboot = new McuBoot();

				if(file.Length <= 0 || !mcuboot.Parse(file.OpenReadStream()))
				{
					logger.LogWarning("Could not parse firmware file");
					continue;
				}

				string path = fw_dir + mcuboot.mfg_board_fw.ToLowerInvariant();

				using(var stream = System.IO.File.Create(path))
				{
					logger.LogInformation("Saved firmware to {0}", path);
					await file.CopyToAsync(stream);
					await hub.Clients.All.NotifyNewFirmware(mcuboot.mfg_board_fw);
				}
			}

			return Ok(new { count = files.Count, size });
		}

		[HttpPost]
		[Route("update/start")]
		public async Task<IActionResult> UpdateStart(
			[FromQuery] string ip,
			[FromQuery] bool all = false)
		{
			// await fwService.UpdateStartTest(ip);
			await fwService.UpdateStart(ip);
			return Ok();
		}

		[HttpPost]
		[Route("update/stop")]
		public ActionResult UpdateStop(
			[FromQuery] string ip,
			[FromQuery] bool all = false)
		{
			fwService.UpdateStop(ip);
			return Ok();
		}

		[HttpGet]
		[Route("update/status")]
		public ActionResult<List<FirmwareUpdateTask>> UpdateStatus()
		{
			return fwService.UpdateStatus();
		}
	}

	public class FirmwareService : BackgroundService
	{
		private SemaphoreSlim taskSem { get; set; } = new SemaphoreSlim(0);
		private ConcurrentQueue<FirmwareUpdateTask> tasks { get; set; } = new ConcurrentQueue<FirmwareUpdateTask>();
		private ILogger<FirmwareService> logger { get; set; }
		private IHubContext<FirmwareUpdateHub, IFirmwareUpdateHub> hub { get; set; }
		private IServiceScopeFactory scopeFactory { get; set; }
		private DatabaseOptions options { get; set; }

		public FirmwareService(
			ILogger<FirmwareService> logger,
			IHubContext<FirmwareUpdateHub, IFirmwareUpdateHub> hub,
			IServiceScopeFactory scopeFactory,
			IOptions<DatabaseOptions> options)
		{
			this.logger       = logger;
			this.hub          = hub;
			this.scopeFactory = scopeFactory;
			this.options      = options.Value;
		}

		public List<string> LatestFirmware()
		{
			var files  = Directory.GetFiles("firmware");
			var latest = files
				.Select(s => new FirmwareString(s))
				.OrderBy(fw => fw)
				.GroupBy(fw => $"{fw.Manuf},{fw.Board}")
				.Select(group => group.Last())
				.Select(fw => fw.ToString()).ToList();

			return latest;
		}

		public string LatestFirmware(FirmwareString firmware)
		{
			var files  = Directory.GetFiles("firmware");
			var latest = files
				.Select(s => new FirmwareString(s))
				.OrderBy(fw => fw)
				.GroupBy(fw => $"{fw.Manuf},{fw.Board}")
				.Select(group => group.Last())
				.FirstOrDefault(fw => fw.Manuf == firmware.Manuf && fw.Board == firmware.Board);

			return latest.ToString();
		}

		// public async Task StartAll()
		// {

		// }

		// public async Task StopAll()
		// {

		// }

		// public async Task UpdateStartTest(string ip)
		// {
		// 	/* Ensure that an update task is not already running */
		// 	var task = tasks.FirstOrDefault(t => t.ip == ip &&
		// 		(!t.IsCancellationRequested ||
		// 		 t.state != FirmwareUpdateTask.UpdateState.Complete ||
		// 		 t.state != FirmwareUpdateTask.UpdateState.Error));

		// 	if(task != null)
		// 	{
		// 		logger.LogInformation($"Update for {ip} already pending");
		// 		return;
		// 	}

		// 	tasks.Enqueue(new FirmwareUpdateTask(ip, this));
		// 	taskSem.Release();
		// }

		public async Task UpdateStart(string ip)
		{
			/* Ensure that an update task is not already running */
			var task = tasks.FirstOrDefault(t => t.ip == ip &&
				(!t.IsCancellationRequested ||
				 t.state != FirmwareUpdateTask.UpdateState.Complete ||
				 t.state != FirmwareUpdateTask.UpdateState.Error));

			if(task != null)
			{
				logger.LogInformation($"Update for {ip} already pending");
				return;
			}

			using(var scope = scopeFactory.CreateScope())
			{
				/* Get device info for the device with the specified IP */
				var dbContext = scope.ServiceProvider.GetRequiredService<HyperspaceDbContext>();
				var device    = await dbContext.Devices.FindAsync(ip);

				if(device == null)
				{
					return;	/* Todo: device not found */
				}

				/* Find the new firmware file that updates the device */
				var latest = LatestFirmware(new FirmwareString(device.image_0));

				if(latest == null)
				{
					return;	/* Todo: no firmware file available */
				}
				else if(latest == device.image_0)
				{
					return;	/* Todo: device already up-to-date */
				}

				/* Enqueue the update task */
				tasks.Enqueue(new FirmwareUpdateTask(
					ip,
					latest,
					scope.ServiceProvider.GetService<ILogger<FirmwareUpdateTask>>(),
					scope.ServiceProvider.GetService<IHubContext<FirmwareUpdateHub, IFirmwareUpdateHub>>()));

				taskSem.Release();
			}
		}

		public void UpdateStop(string ip)
		{
			/* Search for an update task for the specified ip */
			var task = tasks.FirstOrDefault(t => t.ip == ip &&
				(!t.IsCancellationRequested ||
				 t.state != FirmwareUpdateTask.UpdateState.Complete ||
				 t.state != FirmwareUpdateTask.UpdateState.Error));

			if(task != null)
			{
				task.Stop();
			}
		}

		public List<FirmwareUpdateTask> UpdateStatus()
		{
			return tasks.ToList();
		}

		protected override async Task ExecuteAsync(CancellationToken token)
		{
			while(!token.IsCancellationRequested)
			{
				FirmwareUpdateTask task;

				await taskSem.WaitAsync();

				if(tasks.TryDequeue(out task) && !task.IsCancellationRequested)
				{
					// await task.StartTest();
					await task.Start();
				}
			}
		}
	}

	public class FirmwareUpdateTask
	{
		public enum UpdateState { Init, Pending, Updating, Confirming, Complete, Error }

		public string           ip       { get; private set; }
		public string           fwstr    { get; private set; }	/* kh,mesh-root,1.1 */
		[JsonConverter(typeof(JsonStringEnumConverter))]
		public UpdateState      state    { get; private set; }
		public float            progress { get; private set; }

		[JsonIgnore]
		public bool             IsCancellationRequested { get { return source.Token.IsCancellationRequested; }}

		private CancellationTokenSource source = new CancellationTokenSource();
		private ILogger<FirmwareUpdateTask> logger { get; set; }
		private IHubContext<FirmwareUpdateHub, IFirmwareUpdateHub> hub { get; set; }

		public FirmwareUpdateTask(
			string ip,
			string fwstr,
			ILogger<FirmwareUpdateTask> logger,
			IHubContext<FirmwareUpdateHub, IFirmwareUpdateHub> hub)
		{
			this.ip       = ip;
			this.fwstr    = fwstr;
			this.logger   = logger;
			this.hub      = hub;
			this.state    = UpdateState.Pending;
			this.progress = 0.0f;
		}

		private async Task SetState(UpdateState newstate)
		{
			if(newstate != state)
			{
				state = newstate;
				await NotifyProgress();
			}
		}

		// public async Task StartTest()
		// {
		// 	try
		// 	{
		// 		await SetState(UpdateState.Updating);
		// 		while(progress < 100.0f)
		// 		{
		// 			await Task.Delay(1000, source.Token);
		// 			// await Task.Delay(500, source.Token);
		// 			progress += 10.0f;
		// 			await NotifyProgress();
		// 		}

		// 		await SetState(UpdateState.Complete);
		// 	}
		// 	catch(OperationCanceledException e)
		// 	{
		// 		await SetState(UpdateState.Init);
		// 	}
		// }

		public async Task Start()
		{
			var tcs = new TaskCompletionSource();
			var cfg = new CoapConfig();
			cfg.DefaultBlockSize = 64;

			var payload = File.ReadAllBytes($"firmware/{fwstr}");
			var request = new Request(Method.PUT);
			request.URI = new Uri($"coap://[{ip}]/firmware?{fwstr}");
			request.SetPayload(payload, MediaType.ApplicationOctetStream);

			var ep = new CoAPEndPoint(cfg);
			ep.Start();

			ep.SendingRequest += async (s, e) =>
			{
				var block = e.Message.Block1;

				var new_progress = (float)(block.NUM*block.Size)/(float)(payload.Length)*100.0f;

				if(new_progress > progress + 0.09)
				{
					progress = new_progress;
					await NotifyProgress();
				}
			};

			// ep.ReceivingResponse += (s, e) => { Console.WriteLine("ReceivingResponse"); };

			request.Respond += async (s, e) =>
			{
				/* Todo: need to send a command to actually commit the update on the node */
				await SetState(UpdateState.Complete);
			};

			// request.Responding += (s, e) =>
			// {
			// 	Console.WriteLine("Responding: " + e.Response.ResponseText);
			// };

			request.TimedOut += async (s, e) =>
			{
				await SetState(UpdateState.Error);
			};

			await SetState(UpdateState.Updating);

			request.Send(ep);
		}

		public void Stop()
		{
			source.Cancel();
		}

		private async Task NotifyProgress()
		{
			logger.LogDebug($"Update progress: {ip} {state} {progress}");

			await hub.Clients.All.NotifyProgress(this);
		}
	}
}
