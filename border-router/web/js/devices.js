/* Please see documentation at
 *
 * 		https://docs.microsoft.com/aspnet/core/client-side/bundling-and-minification
 *
 * for details on configuring this project to bundle and minify static web assets. */
import * as signalR from "@microsoft/signalr";

"use strict";


/* Global Variables ------------------------------------------------------------------------------ */
var update_stream   = new signalR.HubConnectionBuilder().withUrl("/s/stream").build();
var firmware_stream = new signalR.HubConnectionBuilder().withUrl("/s/firmware").build();
var firmware_files  = [];
var devices         = [];

class Device {
	constructor(data) {
		Object.assign(this, data);

		/* Create the firmware update button */
		let self = this;
		this.btn = document.createElement("button");
		this.btn.innerHTML = "Update";
		this.btn.onclick = async () => { await self.fw_update_btn_on_click_handler(); }
		// this.btn.addEventListener("onclick", async () => {
		// 	await self.fw_update_btn_on_click_handler();
		// });

		/* Keep track of firmware update state */
		this.fw_state = "init";

		/* Add a row to the table */
		let table = document.getElementById("devices-table");
		this.rowhtml = table.insertRow(-1);
		this.rowhtml.insertCell(0).appendChild(document.createTextNode(this.ip));
		this.rowhtml.insertCell(1).appendChild(document.createTextNode(this.updated_at));
		this.rowhtml.insertCell(2).appendChild(document.createTextNode(this.r));
		this.rowhtml.insertCell(3).appendChild(document.createTextNode(this.t));
		this.rowhtml.insertCell(4).appendChild(document.createTextNode(this.seq));
		this.rowhtml.insertCell(5).appendChild(this.btn);
	}

	update(data) {
		Object.assign(this, data);
		this.rowhtml.cells[0].innerText = this.ip;
		this.rowhtml.cells[1].innerText = this.updated_at;
		this.rowhtml.cells[2].innerText = this.r;
		this.rowhtml.cells[3].innerText = this.t;
		this.rowhtml.cells[4].innerText = this.seq;
	}

	// fw_update_new_firmware_uploaded_handler(firmware) {
	//
	// }

	async fw_update_btn_on_click_handler() {
		console.log("onclick " + this.ip);

		let next_state = this.fw_state;
		let base = window.location.origin + "/api/firmware/update";

		switch(this.fw_state.toLowerCase()) {
			case "init":
				next_state = "pending";
				await fetch(base + "/start?ip=" + this.ip, { method: "POST" });
				break;

			case "pending":
				next_state = "init";
				await fetch(base + "/stop?ip=" + this.ip, { method: "POST" });
				break;

			case "updating":
				next_state = "init";
				await fetch(base + "/stop?ip=" + this.ip, { method: "POST" });
				break;

			case "complete":
				break;

			case "error":
				next_state = "pending";
				await fetch(base + "/start?ip=" + this.ip, { method: "POST" });
				break;
		}

		this.#handleNextState(next_state);
	}

	fw_update_progress_handler(data) {
		let next_state = this.fw_state;

		switch(data.state.toLowerCase()) {
			case "init":     next_state = "init";     break;
			case "pending":  next_state = "pending";  break;
			case "updating": next_state = "updating"; this.btn.innerHTML = `Updating (${data.progress.toFixed(1)})`; break;
			case "complete": next_state = "complete"; break;
			case "error":    next_state = "error";    break;
		}

		this.#handleNextState(next_state);
	}

	#handleNextState(next_state) {
		if(next_state == this.fw_state) {
			return;
		}

		this.fw_state = next_state;

		switch(next_state) {
			case "init":
				this.btn.innerHTML = "Update";
				break;

			case "pending":
				this.btn.innerHTML = "Pending";
				break;

			case "updating":
				this.btn.innerHTML = "Updating";
				break;

			case "complete":
				this.btn.innerHTML = "Complete";
				break;

			case "error":
				this.btn.innerHTML = "Error";
				break;
		}
	}
}


/* Main ------------------------------------------------------------------------------------------ */
// test();
main().then();


// function test() {
// 	// let test_devices = [{ ip: "fd00::1", updated_at: "2021-06-11T18:35:23.536422+00:00", r: 0, t: 0, seq: 1 }];
// 	let test_devices = [];

// 	let data = [
// 		{ ip: "fd00::1", updated_at: "2021-06-11T18:35:23.536422+00:00", r: 0, t: 0, seq: 1 },
// 		{ ip: "fd00::2", updated_at: "2021-06-11T18:35:23.536422+00:00", r: 0, t: 0, seq: 1 },
// 		{ ip: "fd00::3", updated_at: "2021-06-11T18:35:23.536422+00:00", r: 0, t: 0, seq: 1 },
// 		{ ip: "fd00::4", updated_at: "2021-06-11T18:35:23.536422+00:00", r: 0, t: 0, seq: 1 },
// 	];

// 	let data2 = [
// 		{ ip: "fd00::3", updated_at: "2021-06-11T18:35:23.536422+00:00", r: 0, t: 0, seq: 2 },
// 		{ ip: "fd00::4", updated_at: "2021-06-11T18:35:23.536422+00:00", r: 0, t: 0, seq: 2 },
// 		{ ip: "fd00::5", updated_at: "2021-06-11T18:35:23.536422+00:00", r: 0, t: 0, seq: 3 },
// 		{ ip: "fd00::6", updated_at: "2021-06-11T18:35:23.536422+00:00", r: 0, t: 0, seq: 3 },
// 	];

// 	update_devices(test_devices, data);
// 	update_devices(test_devices, data2);
// }


// fw-init, fw-pending, fw-updating, fw-complete, fw-error


async function main() {
	/* GET devices from API */
	update_devices(devices, await get_devices());

	/* Listen to devices database updates. Example payload data:
	 *
	 * 		{
	 * 			\"table\" : \"devices\",
	 * 			\"action\" : \"UPDATE\",
	 * 			\"data\" : {
	 * 				\"ip\":\"fd00::1234\",
	 * 				\"updated_at\":\"2021-06-16T22:25:24.520495+00:00\",
	 * 				\"r\":2.5,
	 * 				\"t\":-1.25,
	 * 				\"seq\":null
	 * 			}
	 * 		}
	 */
	update_stream.on("UpdateDevices", payload => {
		console.log("UpdateDevices", payload);
		var obj    = JSON.parse(payload);
		var action = obj.action;
		var device = obj.data;
		update_devices(devices, [device]);
	});

	update_stream.start();

	/* Listen to firmware update progress. Example payload data:
	 *
	 * 		{
	 * 			\"device\": \"fd00::1234\",
	 * 			\"state\": \"Init\", \"Pending\", \"Updating\", \"Complete\", \"Error\"
	 * 			\"progress\": 23.5
	 * 		}
	 */
	firmware_stream.on("NotifyProgress", payload => {
		console.log("NotifyProgress", payload);
		devices.find(c => c.ip == payload.ip)?.fw_update_progress_handler(payload);
	});

	firmware_stream.start();
}

async function get_devices() {
	let response = await fetch(window.location.origin + "/api/devices");
	let data     = await response.json();
	return data;
}

function update_devices(devices, updates) {
	updates.forEach(u => {
		let idx = devices.findIndex(c => c.ip == u.ip);

		if(idx == -1) {
			devices.push(new Device(u));
		} else {
			devices[idx].update(u);
		}
	});
}


function dropHandler(ev) {
	ev.preventDefault();

	var formdata = new FormData();

	if(ev.dataTransfer.items) {
		for(var i = 0; i < ev.dataTransfer.items.length; i++) {
			if(ev.dataTransfer.items[i].kind == "file") {
				var file = ev.dataTransfer.items[i].getAsFile();
				formdata.append("files", file);
			}
		}
	} else {
		for(var i = 0; i < ev.dataTransfer.files.length; i++) {
			formdata.append("files", ev.dataTransfer.files[i]);
		}
	}

	fetch(window.location.origin + "/api/firmware", {
		method: "POST",
		body: formdata,
	});
}


function dragOverHandler(ev) {
	ev.preventDefault();
}
