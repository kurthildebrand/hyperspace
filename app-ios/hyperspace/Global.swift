/************************************************************************************************//**
 * @file		Global.swift
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
import Alamofire
import Foundation
import simd
import SignalRClient


/* var model : ARViewModel
 * var subscribers: [AnyCancellable] = []
 * subscribers.append(model.$doRaycastFlag.receive(on: RunLoop.main).sink(receiveValue: raycast)
 */
class Global: ObservableObject {
    enum State: String {
        case startup
        case startupScanning
        case startupCalibrating
        case normal
    }

    enum Event: String {
        case start
        case scanDone
        case calibDone
    }

    @Published var state: State = .startup
    @Published var networkName  = ""
    @Published var reports      = Set<Report>()
    @Published var calibPoints: [CalibPoint] = []
    @Published var calibReport: CalibReport?

    var coordinator: ARViewContainer.Coordinator?
    var tempCalib: CalibPoint?

    var worldMapIosURL: URL = {
        do {
            return try FileManager.default
                .url(for: .documentDirectory,
                     in: .userDomainMask,
                     appropriateFor: nil,
                     create: true)
                .appendingPathComponent("worldmap.ios")
        } catch {
            fatalError("Can't get worldmap.ios URL: \(error.localizedDescription)")
        }
    }()

    var worldMapObjURL: URL = {
        do {
            return try FileManager.default
                .url(for: .documentDirectory,
                     in: .userDomainMask,
                     appropriateFor: nil,
                     create: true)
                .appendingPathComponent("worldmap.obj")
        } catch {
            fatalError("Can't get worldmap.obj URL: \(error.localizedDescription)")
        }
    }()

    private var signalr: HubConnection

    init() {
        guard let url = URL(string: "http://raspberrypi.local/s/stream") else {
            fatalError("Can't get SignalR URL")
        }

        signalr = HubConnectionBuilder(url: url)
            .withLogging(minLogLevel: .error)
            .withPermittedTransportTypes(.longPolling)
            .build()
        signalr.on(method: "UpdateReports", callback: updateReports)
        signalr.on(method: "UpdateDevices", callback: {})
        signalr.start()
    }

    /*
     ["action": INSERT,
      "table": reports,
      "data": {
         ip = "fd00::cf:e5a0:230d:67af";
         loc =     {
             coordinates =         (
                 0,
                 0,
                 0
             );
             type = Point;
         };
         report =     {
             bindex = 0;
         };
         "updated_at" = "2021-12-03T01:40:40.446247+00:00";
     }])
     */
    private func updateReports(_ payload: String) {
        guard let data = payload.data(using: .utf8) else {
            fatalError("Could not convert payload string to data")
        }

        do {
            let json   = try JSONSerialization.jsonObject(with: data, options: []) as? [String: Any]
            let action = json!["action"] as! String
            let table  = json!["table"]  as! String
            let data   = json!["data"]   as! [String: Any]
            let report = Report(json: data)

            if(action.caseInsensitiveCompare("INSERT") == .orderedSame) {
//                debugPrint(table)
//                debugPrint(report)
            }
        } catch {
            print("JSON serialization went wrong")
        }
    }

    func handle(event: Event, obj: Any? = nil) {
        switch(state) {
        case .startup:
            if(event == .start) {
                /* Steps are as follows:
                 *  1.  Download network info, which includes all current devices in the network.
                 *  2.  Download worldmap.
                 *  3.  Downlaod calibration. */
                self.downloadInfo { infoResponse in
                self.requestDevices { devicesResponse in
                self.requestReports { reportsResponse in
                    /* Handle network information */
                    let json = infoResponse.value as! [String: Any]
                    self.networkName = json["name"] as! String

                    /* Todo: handle devices */
//                    let devices = devicesResponse.value as! [[String: Any]]

                    /* Todo: handle reports */
                    let reportValues = reportsResponse.value as! [[String: Any]]

                    reportValues.forEach { json in
                        self.reports.insert(Report(json: json))
                    }

                    let first = self.reports.first(where: { report in report.ip == "fd00::cf:e5a0:230d:67af" })
                    print(first as Any)

                    self.downloadWorldmap { response in
                        let statusCode = response.response?.statusCode

                        /* If no worldmap, goto the scanning state */
                        if(statusCode == 404) {
                            self.handleNextState(next: .startupScanning)
                            return
                        }

                        self.coordinator?.loadWorldMap()
                        self.downloadCalibration { response in
                            let statusCode = response.response?.statusCode

                            /* If no calibration, goto the calibrating state */
                            if(statusCode == 404) {
                                self.handleNextState(next: .startupCalibrating)
                                return
                            }

                            switch(response.result) {
                            case .success(let calibReport):
                                debugPrint(calibReport)
                                self.calibReport = calibReport
                                self.handleCalibReportReceived(calibReport)
                            case .failure(let error):
                                print(error);
                            }

                            self.handleNextState(next: .normal)
                        }
                    }
                }}}
            }
            break

        case .startupScanning:
            if(event == .scanDone) {
                uploadWorldmap(localPath: worldMapIosURL)
                exportWorldmap(localPath: worldMapObjURL)
                self.handleNextState(next: .startupCalibrating)
            }
            break

        case .startupCalibrating:
            if(event == .calibDone) {
                uploadCalibration()
                self.handleNextState(next: .normal)
            }
            break

        case .normal:
            break
        }
    }

    private func handleNextState(next: State) {
        if(state == next) {
            return
        }

        state = next

        switch(next) {
        case .startupCalibrating:
            self.calibPoints.removeAll()
            break

        default: break
        }
    }



    func handleScanDone() {
        coordinator?.saveWorldMap()
        coordinator?.exportWorldMap()
        handle(event: .scanDone)
    }

    func handleCalibDone() {
        handle(event: .calibDone)
    }

    func handleRaycast() {
        let result = coordinator?.raycast()
        coordinator?.placeTestAnchor(result: result)
    }

    func handleExport() {
        coordinator?.exportWorldMap()
    }

    func handleSave() {
        coordinator?.saveWorldMap()
        uploadWorldmap(localPath: worldMapIosURL)
    }

    func handleLoad() {
        coordinator?.loadWorldMap()
    }

    func calibRaycast() {
        tempCalib  = CalibPoint()
        let result = coordinator?.raycast()

        /* Note: result.worldTransform is a simd_float4x4 */
        tempCalib!.worldLocation = SIMD3<Float>(result!.worldTransform.position)

        print(result!)
    }

    func calibSelected(report: Report) {
        tempCalib!.ip = report.ip
        tempCalib!.reportedLocation = report.loc
        calibPoints.append(tempCalib!)
        print(report.ip)
    }

    func handleCalibReportReceived(_ report: CalibReport?) {
        guard let report = report else { return }
        let transform = report.getTransform()

        /* Todo: clear all anchors from the AR scene */

        /*
         target=estimatedPlane worldTransform=<translation=(-0.198265 -0.979802 -1.650456) rotation=(56.24° -41.08° 0.00°)>>
         fd00::99ee:5147:a6f0:e3f5

         <ARRaycastResult: 0x282b563c0 target=estimatedPlane worldTransform=<translation=(-0.061574 1.087335 -1.732832) rotation=(71.07° -47.84° 0.00°)>>
         fd00::cf:e5a0:230d:67af

         <ARRaycastResult: 0x282b6f4f0 target=estimatedPlane worldTransform=<translation=(-2.417141 -0.387726 0.472844) rotation=(0.35° 108.17° 0.76°)>>
         fd00::97d5:ce12:649b:eaa5

         <ARRaycastResult: 0x282b6dd90 target=estimatedPlane worldTransform=<translation=(1.074867 1.145660 0.784750) rotation=(72.92° 171.30° 180.00°)>>
         fd00::b2cf:8e8:f102:9b6f
         */

        /* For each node... */
        for node in reports {
            /* ...transform the reported location to the global coordinates */
            let lcoord = SIMD4<Float>(node.loc, 1)
//            let gcoord = simd_mul(transform, lcoord)
            let gcoord = transform * lcoord

            /* ...and add an anchor to visualize the node in the AR scene */
            coordinator?.placeTestAnchor(at: gcoord)
        }
    }





    /* ref: https://stackoverflow.com/a/62537480/3780573 */
    private func downloadInfo(completionHandler: @escaping (AFDataResponse<Any>) -> Void) {
        AF.request("http://raspberrypi.local:5000/api/info").responseJSON(completionHandler: completionHandler)
    }

    private func downloadWorldmap(completionHandler: @escaping (AFDataResponse<Data>) -> Void) {
        AF.request("http://raspberrypi.local:5000/api/worldmap/ios").responseData(completionHandler: completionHandler)
    }

//    func downloadCalibration(completionHandler: @escaping (AFDataResponse<Any>) -> Void) {
//        AF.request("http://raspberrypi.local:5000/api/worldmap/calib").responseJSON(completionHandler: completionHandler)
//    }

    private func downloadCalibration(completionHandler: @escaping (AFDataResponse<CalibReport>) -> Void) {
        AF.request("http://raspberrypi.local:5000/api/worldmap/calib").responseDecodable(completionHandler: completionHandler)
    }


    private func uploadWorldmap(localPath: URL) {
        /* Upload data to server */
        /* ref: https://codewithchris.com/alamofire/ */
        AF.upload(multipartFormData: { formData in
            formData.append(localPath, withName: "file")
        },
        to: "http://raspberrypi.local:5000/api/worldmap/ios")
        .responseJSON(completionHandler: { response in
            debugPrint(response)
        })
    }

    private func uploadCalibration() {
        AF.request("http://raspberrypi.local:5000/api/worldmap/calib", method: .post, parameters: calibPoints, encoder: JSONParameterEncoder.default).responseDecodable(of: CalibReport.self) { response in
            switch(response.result) {
            case .success(let calibReport):
                print("Updated calibration")
                debugPrint(calibReport)
                self.calibReport = calibReport
            case .failure(let error):
                print(error)
            }
        }
    }

    private func exportWorldmap(localPath: URL) {
        /* Upload obj file to server */
        AF.upload(multipartFormData: { formData in
            formData.append(localPath, withName: "file")
        },
        to: "http://raspberrypi.local:5000/api/worldmap/obj")
        .responseJSON(completionHandler: { response in
            debugPrint(response)
        })
    }


    private func requestDevices(completionHandler: @escaping (AFDataResponse<Any>) -> Void) {
        AF.request("http://raspberrypi.local:5000/api/devices").responseJSON(completionHandler: completionHandler)
    }

    private func requestReports(completionHandler: @escaping (AFDataResponse<Any>) -> Void) {
        AF.request("http://raspberrypi.local:5000/api/reports").responseJSON(completionHandler: completionHandler)
    }
}
