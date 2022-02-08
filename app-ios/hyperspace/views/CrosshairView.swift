/************************************************************************************************//**
 * @file		CrosshairView.swift
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

struct CrosshairView: View {
    let length = CGFloat(24.0)
    let pad    = CGFloat(0.0)
    let offset = CGFloat(1.0)
    var body: some View {
//        let x = length/2
//        let y = length/2
        ZStack {
            Path { path in
                path.move(to: CGPoint(x: length/2, y: 0))
                path.addLine(to: CGPoint(x: length/2, y: length))

                path.move(to: CGPoint(x: 0, y: length/2))
                path.addLine(to: CGPoint(x: length, y: length/2))
            }
            .stroke(Color.black, lineWidth: 3)

//            Path { path in
//                path.move   (to: CGPoint(x: x+offset, y: length))
//                path.addLine(to: CGPoint(x: x+offset, y: y+offset))
//                path.addLine(to: CGPoint(x: length, y: y+offset))
//            }.stroke(Color.black, lineWidth: 2)
//            Path { path in
//                path.move   (to: CGPoint(x: x-offset, y: length))
//                path.addLine(to: CGPoint(x: x-offset, y: y+offset))
//                path.addLine(to: CGPoint(x: 0, y: y+offset))
//            }.stroke(Color.black, lineWidth: 2)
//            Path { path in
//                path.move   (to: CGPoint(x: 0, y: y-offset))
//                path.addLine(to: CGPoint(x: x-offset, y: y-offset))
//                path.addLine(to: CGPoint(x: x-offset, y: 0))
//            }.stroke(Color.black, lineWidth: 2)
//            Path { path in
//                path.move   (to: CGPoint(x: x+offset, y: 0))
//                path.addLine(to: CGPoint(x: x+offset, y: y-offset))
//                path.addLine(to: CGPoint(x: length, y: y-offset))
//            }.stroke(Color.black, lineWidth: 2)

            Path { path in
                path.move(to: CGPoint(x: length/2, y: pad))
                path.addLine(to: CGPoint(x: length/2, y: length-pad))
                path.move(to: CGPoint(x: pad, y: length/2))
                path.addLine(to: CGPoint(x: length-pad, y: length/2))
            }.stroke(Color.init(white: 0.95), lineWidth: 1.5)
        }
        .frame(width: length, height: length)
    }
}

struct CrosshairView_Previews: PreviewProvider {
    static var previews: some View {
        CrosshairView()
    }
}
