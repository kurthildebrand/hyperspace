/************************************************************************************************//**
 * @file		MulticastTestService.cs
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

// using Npgsql;
// using NpgsqlTypes;
// using Microsoft.Extensions.Hosting;
// using Microsoft.Extensions.Logging;
// using Microsoft.Extensions.Options;
// using System.Net.Sockets;
// using System.Text.Json;
// using System.Threading;
// using System.Threading.Tasks;
// using System.IO;
// using System;
// using NetTopologySuite.Geometries;
// using System.Text;
// using System.Collections.Generic;
// using System.Net;
// using System.Linq;
// using System.Net.NetworkInformation;

// namespace Hyperspace.Services
// {
// 	public class MulticastTestService : BackgroundService
// 	{
// 		private ILogger<MulticastTestService> logger  { get; set; }
// 		private static int PORT = 2201;

// 		public MulticastTestService(ILogger<MulticastTestService> logger)
// 		{
// 			this.logger = logger;
// 		}

// 		protected override async Task ExecuteAsync(CancellationToken token)
// 		{
// 			byte[] msg = "Multicast Test".ToCharArray().Select(c => (byte)c).ToArray();

// 			var interfaces = NetworkInterface.GetAllNetworkInterfaces();
// 			var tun0 = interfaces.FirstOrDefault(i => i.Name == "tun0");

// 			if(tun0 is null)
// 			{
// 				logger.LogError("Could not find tun0");
// 				return;
// 			}

// 			var index = tun0.GetIPProperties()?.GetIPv4Properties()?.Index;

// 			if(!index.HasValue)
// 			{
// 				logger.LogError("Could not find tun0 index");
// 				return;
// 			}


// 			Task.Run(() => MulticastReceive(tun0.Name, index.Value));
// 			Task.Run(() => MulticastTransmit(tun0.Name, index.Value));


// 			// var ep = IPAddress.Parse("FF22::2");
// 			// var socket = new Socket(AddressFamily.InterNetworkV6, SocketType.Dgram, ProtocolType.Udp);
// 			// socket.SetSocketOption(SocketOptionLevel.Socket, SocketOptionName.ReuseAddress, true);
// 			// socket.SetSocketOption(SocketOptionLevel.IPv6, SocketOptionName.MulticastTimeToLive, 64);
// 			// socket.SetSocketOption(SocketOptionLevel.IPv6, SocketOptionName.AddMembership,
// 			// 	new IPv6MulticastOption(ep, index.Value));
// 			// socket.Bind(new IPEndPoint(IPAddress.IPv6Any, 0));
// 			// socket.SendTo(msg, new IPEndPoint(ep, PORT));



// 			// /* Set UDP socket */
// 			// var udp  = new UdpClient(PORT, AddressFamily.InterNetworkV6);
// 			// // var grp  = IPAddress.Parse("FF01::3");
// 			// var grp  = IPAddress.Parse("FF22::2");
// 			// var dest = new IPEndPoint(grp, PORT);

// 			// // IPv6MulticastOption ipv6MulticastOption = new IPv6MulticastOption(grp);
// 			// IPv6MulticastOption ipv6MulticastOption = new IPv6MulticastOption(
// 			// 	grp, (long)index.Value);

// 			// udp.JoinMulticastGroup(index.Value, grp);

// 			// while(!token.IsCancellationRequested)
// 			// {
// 			// 	await Task.Delay(1000);

// 			// 	// var ret = await udp.SendAsync(msg, msg.Length, dest);
// 			// 	var ret = udp.Send(msg, msg.Length, dest);

// 			// 	logger.LogInformation("mcast ret = {0}", ret);
// 			// }
// 		}

// 		private async Task MulticastReceive(string ifname, int ifindex)
// 		{
// 			var udp  = new UdpClient(2201, AddressFamily.InterNetworkV6);
// 			// var addr = IPAddress.Parse("FF22::2%tun0");
// 			var addr = IPAddress.Parse(String.Format("FF22::2%{0}", ifname));
// 			var ep   = new IPEndPoint(IPAddress.Any, 2201);

// 			udp.JoinMulticastGroup(ifindex, addr);

// 			// Byte[] bytes = udp.Receive(ref ep);
// 			// string str = ASCIIEncoding.ASCII.GetString(bytes);

// 			while(true)
// 			{
// 				var    res = await udp.ReceiveAsync();
// 				string str = ASCIIEncoding.ASCII.GetString(res.Buffer);

// 				logger.LogInformation("Multicast Receive: {0}", str);
// 			}
// 		}

// 		private async Task MulticastTransmit(string ifname, int ifindex)
// 		{
// 			byte[] msg = "Multicast Test".ToCharArray().Select(c => (byte)c).ToArray();

// 			var udp    = new UdpClient(2202, AddressFamily.InterNetworkV6);
// 			var addr   = IPAddress.Parse(String.Format("FF22::2%{0}", ifname));
// 			var target = new IPEndPoint(addr, 2201);

// 			udp.JoinMulticastGroup(ifindex, addr);

// 			udp.Client.SetSocketOption(SocketOptionLevel.IPv6, SocketOptionName.MulticastTimeToLive, 64);
// 			// udp.Ttl = 64;

// 			while(true)
// 			{
// 				await Task.Delay(5000);
// 				var ret = await udp.SendAsync(msg, msg.Length, target);

// 				logger.LogInformation("Multicast Transmit: {0}", ret);
// 			}
// 		}
// 	}
// }
