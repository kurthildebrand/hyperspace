/************************************************************************************************//**
 * @file		IndicatorEntity.swift
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
import ARKit
import RealityKit

struct CameraBillboardComponent: Component, Codable {}

protocol HasCameraBillboard where Self: Entity {}

extension HasCameraBillboard {
    var cameraBillboard: CameraBillboardComponent {
        get { return components[CameraBillboardComponent.self] ?? CameraBillboardComponent() }
        set { components[CameraBillboardComponent.self] = newValue }
    }

    func update(targetPosition: SIMD3<Float>) {
        look(at: targetPosition, from: position(relativeTo: nil), relativeTo: nil)
    }
}

class IndicatorAnchor: Entity, HasAnchoring, HasCameraBillboard {
//    var report: Report

    required init() {
        fatalError("init() has not been implemented")
    }

    init(world: float4x4) {
        super.init()

        self.components[AnchoringComponent] = AnchoringComponent(
            AnchoringComponent.Target.world(transform: world))

        let textHeight: CGFloat = 0.05
        let textMesh = MeshResource.generateText(
            "Test",
            extrusionDepth: 0.000,
            font: .init(descriptor: .init(name: "Helvetica", size: textHeight), size: textHeight))

        let textModel = ModelEntity(mesh: textMesh, materials: [UnlitMaterial(color: .white)])

        textModel.scale = .one * 2
        textModel.orientation = simd_quatf(angle: .pi, axis: [0,1,0])
        self.addChild(textModel)
    }

    convenience init(world: SIMD3<Float>) {
        self.init(world: float4x4(
            SIMD4<Float>(1, 0, 0, 0),
            SIMD4<Float>(0, 1, 0, 0),
            SIMD4<Float>(0, 0, 1, 0),
            SIMD4<Float>(world, 1)))
    }
}

//class IndicatorEntity: Entity, HasModel, HasCameraBillboard {
//    required init() {
//        super.init()
//
//        self.cameraBillboard = CameraBillboardComponent()
//
//        let textHeight: CGFloat = 0.05
//        let textMesh = MeshResource.generateText(
//            "Test",
//            extrusionDepth: 0.000,
//            font: .init(descriptor: .init(name: "Helvetica", size: textHeight), size: textHeight))
//
//        let textModel = ModelEntity(mesh: textMesh, materials: [UnlitMaterial(color: .blue)])
//
//        textModel.scale = .one * 2
//        textModel.orientation = simd_quatf(angle: .pi, axis: [0,1,0])
//        self.addChild(textModel)
//    }
//}
