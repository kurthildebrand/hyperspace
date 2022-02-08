/************************************************************************************************//**
 * @file		NormalView.swift
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

struct NormalView: View {
    @ObservedObject var global: Global
    var body: some View {
        VStack {
            Text("\(global.state)" as String)
                .foregroundColor(.white)
            Spacer()
            HStack {
                Button(action: global.handleRaycast) {
                    Text("Ray")
                }.buttonStyle(ARBasicButtonStyle())
                Button(action: global.handleExport) {
                    Text("Export")
                }.buttonStyle(ARBasicButtonStyle())
                Button(action: global.handleSave) {
                    Text("Save")
                }.buttonStyle(ARBasicButtonStyle())
                Button(action: global.handleLoad) {
                    Text("Load")
                }.buttonStyle(ARBasicButtonStyle())
            }
        }
    }
}

struct NormalView_Previews: PreviewProvider {
    static var global = Global()
    static var previews: some View {
        NormalView(global: global)
    }
}
