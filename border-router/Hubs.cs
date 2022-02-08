/************************************************************************************************//**
 * @file		Hubs.cs
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
using Microsoft.AspNetCore.SignalR;
using System.Threading.Tasks;
using static Hyperspace.FirmwareService;

namespace Hyperspace.Hubs
{
	public interface IDbStream
	{
		Task UpdateDevices(string json);
		Task UpdateReports(string json);
	}

	public class DbStreamHub : Hub<IDbStream>
	{
		public async Task SendUpdatedDevices(string json)
		{
			await Clients.All.UpdateDevices(json);
		}

		public async Task SendUpdatedReports(string json)
		{
			await Clients.All.UpdateReports(json);
		}
	}

	public interface IFirmwareUpdateHub
	{
		// Task StartUpdate(string json);
		// Task StartUpdateAll();
		// Task StopUpdate(string json);
		// Task StopUpdateAll();
		// Task UpdateProgress(string json);
		// Task NotifyProgress(string json);
		Task NotifyProgress(FirmwareUpdateTask task);
		Task NotifyNewFirmware(string firmware);
	}

	public class FirmwareUpdateHub : Hub<IFirmwareUpdateHub>
	{
		// public async Task StartUpdate(string json)
		// {

		// }
	}
}
