/************************************************************************************************//**
 * @file		StartupCalibratingView.swift
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
import SwiftUI

struct StartupCalibratingView: View {
    @ObservedObject var global: Global
    @State var isPresented = false

    var body: some View {
        /*  1.  Raycast Button. On press, raycast and get world location
         *  2.      Popup to select the IP address of targeted node
         *  3.      Store IP address, reported location, and world location
         *  4.  Repeat until at least 3 nodes have been saved
         *  5.  Done Button. Displayed when at least 3 nodes have been saved
         *  6.      On press, upload IP addresses, reported locations, and world locations to server
         *  7.      Server response shall contain the translation and rotation matrices to turn node reported locations into world locations
         *  8.  Goto normal view
         */
        VStack {
            Text("\(global.state)" as String)
                .foregroundColor(.white)
            Spacer()
            HStack {
                Button(action: {
                    global.calibRaycast()
                    isPresented = true;
                }) {
                    Text("Ray")
                }.buttonStyle(ARBasicButtonStyle())
                Button(action: global.handleCalibDone) {
                    Text("Done Calibrating")
                }.buttonStyle(ARBasicButtonStyle())
            }
        }
        .sheet(isPresented: $isPresented) {
            NavigationView {
                List {
                    ForEach(global.reports.sorted(by: >), id: \.ip) { report in
                        Button(action: {
                            global.calibSelected(report: report)
                            isPresented = false
                        }) {
                            Text(report.ip)
                        }
                    }
                }
                .navigationTitle("Select Device")
                .navigationBarItems(leading: Button("Cancel") {
                    isPresented = false
                })
            }
        }
    }
}

struct StartupCalibratingView_Previews: PreviewProvider {
    static var global = Global()
    static var previews: some View {
        StartupCalibratingView(global: global)
    }
}
