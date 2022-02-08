/************************************************************************************************//**
 * @file		ContentView.swift
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
import SwiftUI
import Combine
import MetalKit
import ModelIO
import QuickLook
//import FocusEntity
import Alamofire

struct ContentView : View {
    @ObservedObject var global: Global
    @State var isPresented = false

    init(global: Global) {
        self.global = global
    }

    var body: some View {
        ZStack {
            ARViewContainer(global: global)
                .edgesIgnoringSafeArea(/*@START_MENU_TOKEN@*/.all/*@END_MENU_TOKEN@*/)
                .onAppear { UIApplication.shared.isIdleTimerDisabled = true }
                .onDisappear { UIApplication.shared.isIdleTimerDisabled = false }

            CrosshairView()

            if(global.state == .startupScanning) {
                StartupScanningView(global: global)
            } else if(global.state == .startupCalibrating) {
                StartupCalibratingView(global: global)
            } else if(global.state == .normal) {
                NormalView(global: global)
            } else {
                VStack {
                    Text("\(global.state)" as String)
                        .foregroundColor(.white)
                    Spacer()
                }
            }
        }
    }
}


struct ARBasicButtonStyle: ButtonStyle {
//    func makeBody(configuration: Self.Configuration) -> some View {
//        configuration.label
//            .foregroundColor(Color.white)
//            .padding()
//            .background(LinearGradient(gradient: Gradient(colors: [Color.red, Color.orange]), startPoint: .leading, endPoint: .trailing))
//            .cornerRadius(15.0)
//            .scaleEffect(configuration.isPressed ? 1.3 : 1.0)
//    }

    func makeBody(configuration: Self.Configuration) -> some View {
        configuration.label
            .foregroundColor(.white)
            .padding()
            .background(Color.accentColor)
            .cornerRadius(8)
    }
}


struct ARViewContainer: UIViewRepresentable {
    @ObservedObject var global: Global

    func makeCoordinator() -> Coordinator {
        Coordinator(self, global: global)
    }

    func makeUIView(context: Context) -> ARView {
        CameraBillboardComponent.registerComponent()

        let arView = ARView(frame: .zero)
        context.coordinator.setupARView(arView: arView)
        arView.session.delegate = context.coordinator
        return arView
    }

    func updateUIView(_ uiView: ARView, context: UIViewRepresentableContext<ARViewContainer>) {}
}


extension ARViewContainer {
    final class Coordinator: NSObject, ObservableObject, ARSessionDelegate {
        var parent: ARViewContainer
        var arView: ARView?
//        var focusSquare: FocusEntity?
//        var entities: [Entity] = [] // @Todo: replace with system in RealityKit 2.0
        var subscribers: [AnyCancellable] = []

        @ObservedObject var global: Global

        init(_ parent: ARViewContainer, global: Global) {
            self.parent = parent
            self.global = global
            super.init()

            global.coordinator = self
        }

        deinit {
            for sub in subscribers {
                sub.cancel()
            }
        }

        func setupARView(arView: ARView, options: ARSession.RunOptions? = nil, worldMap: ARWorldMap? = nil) {
            self.arView = arView
//            self.focusSquare = FocusEntity(on: arView, focus: .classic)

            #if !targetEnvironment(simulator)
            self.arView?.environment.sceneUnderstanding.options = []

            /* Turn on occlusion from the scene reconstruction's mesh. */
            self.arView?.environment.sceneUnderstanding.options.insert(.occlusion)

            /* Turn on physics for the scene reconstruction's mesh. */
            self.arView?.environment.sceneUnderstanding.options.insert(.physics)

            /* Display a debug visualization of the mesh. */
            self.arView?.debugOptions.insert(.showSceneUnderstanding)

            /* For performance, disable render options that are not required for this app. */
            self.arView?.renderOptions = [.disablePersonOcclusion, .disableDepthOfField, .disableMotionBlur]

            /* Manually configure what kind of AR session to run since ARView on its own does not turn on mesh classification. */
            self.arView?.automaticallyConfigureSession = false
            let configuration = ARWorldTrackingConfiguration()
            configuration.sceneReconstruction = .meshWithClassification
            configuration.planeDetection = [.horizontal, .vertical]
            configuration.environmentTexturing = .automatic

            /* Set initial WorldMap if it's provided */
            if(worldMap != nil) {
                configuration.initialWorldMap = worldMap!
            }

            if(options != nil) {
                self.arView?.session.run(configuration, options: options!)
            } else {
                self.arView?.session.run(configuration)
            }

            self.arView?.scene
                .publisher(for: SceneEvents.Update.self)
                .eraseToAnyPublisher()
                .sink { [weak self] (event) in
                    self?.update(deltaTime: event.deltaTime)
                }
                .store(in: &self.subscribers)
            #endif
        }

        private func update(deltaTime: TimeInterval) {
            forEachEntity(where: { entity in
                guard let e = entity as? HasCameraBillboard else { return }
                e.update(targetPosition: arView!.cameraTransform.translation)
            })
        }

        private func forEachEntity(where predicate: (Entity) -> Void) {
            arView?.scene.anchors.forEach {
                forEachEntity(entity: $0, where: predicate)
            }
        }

        private func forEachEntity(entity: Entity, where predicate: (Entity) -> Void) {
            predicate(entity)
            for child in entity.children {
                forEachEntity(entity: child, where: predicate)
            }
        }

        func raycast() -> ARRaycastResult? {
            let screenCenter = CGPoint(x: self.arView!.bounds.midX, y: self.arView!.bounds.midY)
            return self.arView!.raycast(from: screenCenter, allowing: .estimatedPlane, alignment: .any).first;
        }

        func placeTestAnchor(at: SIMD4<Float>) {
            let loc    = SIMD3<Float>(at.x, at.y, at.z)
//            let anchor = AnchorEntity(world: loc)
//            let anchor = AnchorEntity(world)
//            let entity = IndicatorEntity()
//            anchor.addChild(entity)
//            arView!.scene.addAnchor(anchor)

            let anchor = IndicatorAnchor(world: loc)
            arView!.scene.addAnchor(anchor)
        }

        func placeTestAnchor(result: ARRaycastResult?) {
            guard let result = result else {
                return
            }

            let rayDirection = normalize(result.worldTransform.position - self.arView!.cameraTransform.translation)
            let textPositionInWorldCoordiantes = result.worldTransform.position - (rayDirection * 0.1)
            var resultWithCameraOrientation = self.arView!.cameraTransform
            resultWithCameraOrientation.translation = textPositionInWorldCoordiantes

            let anchor = IndicatorAnchor(world: resultWithCameraOrientation.matrix)

            arView!.scene.addAnchor(anchor)

//            let anchor = AnchorEntity(world: resultWithCameraOrientation.matrix)
//            anchor.addChild(IndicatorEntity())
//            arView!.scene.addAnchor(anchor)
            print(result)
        }

        func saveWorldMap() {
            #if !targetEnvironment(simulator)
            arView?.session.getCurrentWorldMap { (worldMap, error) in
                guard let worldMap = worldMap else {
                    print("Can't get current world map")
                    return
                }
                do {
                    let data = try NSKeyedArchiver.archivedData(withRootObject: worldMap, requiringSecureCoding: false)
                    try data.write(to: self.global.worldMapIosURL, options: [.atomic])

//                    /* Debug: export the file to inspect it */
//                    let activityController = UIActivityViewController(activityItems: [self.worldMapURL], applicationActivities: nil)
//                    activityController.popoverPresentationController?.sourceView = self.arView
//                    UIApplication.shared.windows.first?.rootViewController?.present(activityController, animated: true, completion: nil)
                    print("World map saved to \(self.global.worldMapIosURL)")
                } catch {
                    fatalError("Can't save map: \(error.localizedDescription)")
                }
            }
            #endif
        }

        func loadWorldMap() {
            guard let mapData = try? Data(contentsOf: self.global.worldMapIosURL), let worldMap = try? NSKeyedUnarchiver.unarchivedObject(ofClass: ARWorldMap.self, from: mapData) else {
                fatalError("No ARWorldMap in archive.")
            }

            let options: ARSession.RunOptions = [.resetTracking, .removeExistingAnchors]

            setupARView(arView: self.arView!, options: options, worldMap: worldMap)

            print("World map loaded from \(self.global.worldMapIosURL)")
        }

        /* Ref: https://github.com/zeitraumdev/iPadLIDARScanExport/blob/master/iPadLIDARScanExport/ViewController.swift */
        func exportWorldMap() {
            guard let frame = arView?.session.currentFrame else {
                fatalError("Couldn't get the current ARFrame")
            }

            /* Fetch the default MTLDevice to initialize a MetalKit buffer allocator */
            guard let device = MTLCreateSystemDefaultDevice() else {
                fatalError("Failed to get the system's default Metal device!")
            }

            /* Use Model I/O framework to export the scan. Initialize MDLAsset object, which we can export to a file later, with a buffer allocator */
            let allocator = MTKMeshBufferAllocator(device: device)
            let asset = MDLAsset(bufferAllocator: allocator)

            /* Fetch all ARMeshAnchors */
            let meshAnchors = frame.anchors.compactMap({ $0 as? ARMeshAnchor })

            /* Convert the geometry of each ARMeshAnchor into a MDLMesh and add it to the MDLAsset */
            for meshAncor in meshAnchors {
                /* Some short handles, otherwise stuff will get pretty long in a few lines */
                let geometry = meshAncor.geometry
                let vertices = geometry.vertices
                let faces = geometry.faces
                let verticesPointer = vertices.buffer.contents()
                let facesPointer = faces.buffer.contents()

                /* Convert each vertex of the geometry from the local space of their ARMeshAnchor to world space */
                for vertexIndex in 0..<vertices.count {
                    /* Extract the current vertex with an extension method provided by Apple in Extensions.swift */
                    let vertex = geometry.vertex(at: UInt32(vertexIndex))

                    /* Build a transform matrix with only the vertex position and apply the mesh anchors transform to convert it into world space */
                    var vertexLocalTransform = matrix_identity_float4x4
                    vertexLocalTransform.columns.3 = SIMD4<Float>(x: vertex.0, y: vertex.1, z: vertex.2, w: 1)
                    let vertexWorldPosition = (meshAncor.transform * vertexLocalTransform).position

                    /* Write the world space vertex back into its position in the vertex buffer */
                    let vertexOffset = vertices.offset + vertices.stride * vertexIndex
                    let componentStride = vertices.stride / 3
                    verticesPointer.storeBytes(of: vertexWorldPosition.x, toByteOffset: vertexOffset, as: Float.self)
                    verticesPointer.storeBytes(of: vertexWorldPosition.y, toByteOffset: vertexOffset + componentStride, as: Float.self)
                    verticesPointer.storeBytes(of: vertexWorldPosition.z, toByteOffset: vertexOffset + (2 * componentStride), as: Float.self)
                }

                /* Initialize MDLMeshBuffers with the content of the vertex and face MTLBuffers */
                let byteCountVertices = vertices.count * vertices.stride
                let byteCountFaces = faces.count * faces.indexCountPerPrimitive * faces.bytesPerIndex
                let vertexBuffer = allocator.newBuffer(with: Data(bytesNoCopy: verticesPointer, count: byteCountVertices, deallocator: .none), type: .vertex)
                let indexBuffer = allocator.newBuffer(with: Data(bytesNoCopy: facesPointer, count: byteCountFaces, deallocator: .none), type: .index)

                /* Create a MDLSubMesh with the index buffer and a generic material */
                let indexCount = faces.count * faces.indexCountPerPrimitive
                let material = MDLMaterial(name: "mat1", scatteringFunction: MDLPhysicallyPlausibleScatteringFunction())
                let submesh = MDLSubmesh(indexBuffer: indexBuffer, indexCount: indexCount, indexType: .uInt32, geometryType: .triangles, material: material)

                /* Create a MDLVertexDescriptor to describe the memory layout of the mesh */
                let vertexFormat = MTKModelIOVertexFormatFromMetal(vertices.format)
                let vertexDescriptor = MDLVertexDescriptor()
                vertexDescriptor.attributes[0] = MDLVertexAttribute(name: MDLVertexAttributePosition, format: vertexFormat, offset: 0, bufferIndex: 0)
                vertexDescriptor.layouts[0] = MDLVertexBufferLayout(stride: meshAncor.geometry.vertices.stride)

                /* Finally, create the MDLMesh and add it to the MDLAsset */
                let mesh = MDLMesh(vertexBuffer: vertexBuffer, vertexCount: meshAncor.geometry.vertices.count, descriptor: vertexDescriptor, submeshes: [submesh])
                asset.add(mesh)
            }

//            /* Setting the OBJ file export path */
//            let documentsPath = FileManager.default.urls(for: .documentDirectory, in: .userDomainMask).first!
//            let objUrl = documentsPath.appendingPathComponent("worldmap.obj")

            /* Export the OBJ file */
            if(!MDLAsset.canExportFileExtension("obj")) {
                fatalError("Can't export OBJ")
            }

            do {
                try asset.export(to: self.global.worldMapObjURL)

                print("Exported world map to \(self.global.worldMapObjURL)")

//                /* Share the OBJ file */
//                let activityController = UIActivityViewController(activityItems: [objUrl], applicationActivities: nil)
//                activityController.popoverPresentationController?.sourceView = arView
//                UIApplication.shared.windows.first?.rootViewController?.present(activityController, animated: true, completion: nil)
            } catch let error {
                fatalError(error.localizedDescription)
            }
        }

        func sessionWasInterrupted(_ session: ARSession) {}

        func sessionInterruptionEnded(_ session: ARSession) {}

        func session(_ session: ARSession, cameraDidChangeTrackingState camera: ARCamera) {}
    }
}


#if DEBUG
struct ContentView_Previews : PreviewProvider {
    static var global = Global()
    static var previews: some View {
        ContentView(global: global)
    }
}
#endif
