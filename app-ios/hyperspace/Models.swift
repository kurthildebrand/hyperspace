/************************************************************************************************//**
 * @file		Models.swift
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
import Foundation
import simd

protocol HyperspaceNode {
//    var ip: IPv6Address { get }
    var ip: String { get }
}


class CalibPoint: Codable {
    var ip: String?
    var reportedLocation: SIMD3<Float>?
    var worldLocation: SIMD3<Float>?
}


class CalibReport: Codable {
//    var transform: simd_float4x4
    var rotation: [[Float]]
    var translation: [Float]
    var scale: Float;

    /* Calibration is applied Q = t + c*R*P, where:
     *  t is translation
     *  c is scale
     *  R is rotation
     *  P is the uncalibrated point or matrix of points. */
    func getTransform() -> simd_float4x4 {
        return simd_float4x4(columns: (
            SIMD4<Float>(scale*rotation[0][0], scale*rotation[1][0], scale*rotation[2][0], 0),
            SIMD4<Float>(scale*rotation[0][1], scale*rotation[1][1], scale*rotation[2][1], 0),
            SIMD4<Float>(scale*rotation[0][2], scale*rotation[1][2], scale*rotation[2][2], 0),
            SIMD4<Float>(translation[0], translation[1], translation[2], 1)
        ))
    }
}


//class Device: {
//    var ip: String
//    var updated_at: Date
//    var r: Float
//    var t: Float
//    var seq: Int
//    var image_0: String
//    var image_1: String
//    var coap_well_known: String
//}


class Report: HyperspaceNode, Hashable, Comparable, ObservableObject {
    var ip: String

    @Published var loc = SIMD3<Float>(x: Float.nan, y: Float.nan, z: Float.nan)
    @Published var updated_at = Date()

    init(json: [String: Any]) {
        let dateFormatter = ISO8601DateFormatter()
        dateFormatter.timeZone = TimeZone(secondsFromGMT: 0)
        dateFormatter.formatOptions = [
            .withFullDate,
            .withFullTime,
            .withDashSeparatorInDate,
            .withFractionalSeconds]

//        ip = IPv6Address(json["ip"] as! String)!
        ip = json["ip"] as! String
        updated_at = dateFormatter.date(from: json["updated_at"] as! String)!

        if let loc = json["loc"] as? [String: Any] {
            if let coords = loc["coordinates"] as? [Any] {
                self.loc.x = Float(coords[0] as! Double)
                self.loc.y = Float(coords[1] as! Double)
                self.loc.z = Float(coords[2] as! Double)
            }
        }
    }

    static func < (lhs: Report, rhs: Report) -> Bool {
        return lhs.ip < rhs.ip
    }

    static func == (lhs: Report, rhs: Report) -> Bool {
        return lhs.ip == rhs.ip
    }

    func hash(into hasher: inout Hasher) {
        hasher.combine(ip)
    }
}
