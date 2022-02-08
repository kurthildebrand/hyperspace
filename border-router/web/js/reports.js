/* Please see documentation at 
 * 
 * 		https://docs.microsoft.com/aspnet/core/client-side/bundling-and-minification
 * 
 * for details on configuring this project to bundle and minify static web assets. */
import * as signalR from "@microsoft/signalr";

"use strict";


/* Global Variables ------------------------------------------------------------------------------ */
var update_stream = new signalR.HubConnectionBuilder().withUrl("/s/stream").build();
var reports = [];


/* Main ------------------------------------------------------------------------------------------ */
main().then();


async function main() {
	/* GET reports from API */
	update_reports(reports, await get_reports());

	update_stream.on("UpdateReports", payload => {
		console.log("Received notification", payload);
		var obj    = JSON.parse(payload);
		var action = obj.action;
		var report = obj.data;
		update_reports(reports, [report]);
	});

	update_stream.start();
}


/* Example reports data from api:
 * 	[
 * 		{
 * 			"ip": "fd00::cf:e5a0:230d:67af",
 * 			"updated_at": "2021-06-09T21:44:52.445979+00:00",
 * 			"loc": {
 * 				"type": "Point",
 * 				"coordinates": [
 * 					0,
 * 					0,
 * 					0
 * 				]
 * 			},
 * 			"report": "{}"
 * 		}
 * 	]
 */
async function get_reports() {
	let response = await fetch(window.location.origin + "/api/reports");
	let data     = await response.json();
	return data;
}


function update_reports(reports, updates) {
	updates.forEach(u => {
		let idx = reports.findIndex(r => r.ip == u.ip);

		if(idx != -1) {
			update_report_row(reports, u, idx);
		} else {
			insert_report_row(reports, u, -1);
		}
	});
}


function update_report_row(reports, update, row) {
	update.rowhtml                    = reports[row].rowhtml;
	update.rowhtml.cells[0].innerText = update.ip;
	update.rowhtml.cells[1].innerText = update.updated_at;
	update.rowhtml.cells[2].innerText = update.loc.coordinates.join(", ");
	update.rowhtml.cells[3].innerText = JSON.stringify(update.report);
	reports[row] = update;
}


function insert_report_row(reports, update, row) {
	let table = document.getElementById("reports-table");
	update.rowhtml = table.insertRow(row);
	update.rowhtml.insertCell(0).appendChild(document.createTextNode(update.ip));
	update.rowhtml.insertCell(1).appendChild(document.createTextNode(update.updated_at));
	update.rowhtml.insertCell(2).appendChild(document.createTextNode(update.loc.coordinates.join(", ")));
	update.rowhtml.insertCell(3).appendChild(document.createTextNode(update.report));
	reports.push(update);
}
