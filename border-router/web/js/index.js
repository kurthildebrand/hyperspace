import * as THREE from "three";
import { OBJLoader } from "three/examples/jsm/loaders/OBJLoader.js";
import { OrbitControls } from "three/examples/jsm/controls/OrbitControls";

let container;
let camera, scene, renderer;
let mouseX = 0, mouseY = 0;

let windowHalfX = window.innerWidth / 2;
let windowHalfY = window.innerHeight / 2;

let controls;

init();
animate();

function init() {

	container = document.createElement('div');
	document.body.appendChild(container);

	camera = new THREE.PerspectiveCamera(45, window.innerWidth / window.innerHeight, 1, 2000);
	camera.position.z = 250;

	scene = new THREE.Scene();
	const ambientLight = new THREE.AmbientLight(0xCCCCCC, 0.4);
	scene.add(ambientLight);
	const pointLight = new THREE.PointLight(0xFFFFFF, 0.8);
	camera.add(pointLight);
	scene.add(camera);

	const textureLoader = new THREE.TextureLoader();
	const texture = textureLoader.load("uv_grid_opengl.jpg");
	var objLoader = new OBJLoader();
	objLoader.load("worldmap.obj", (obj) => {
			// obj.traverse((child) => {
			// 	if(child.isMesh) {
			// 		child.material.map = texture;
			// 	}
			// });

			obj.scale.set(8,8,8);
			scene.add(obj);
		},
		(xhr) => {
			console.log((xhr.loaded / xhr.total) * 100 + '% loaded')
		},
		(error) => {
			console.log(error)
		}
	);



	// function loadModel() {
	// 	object.traverse(function (child) {
	// 		if(child.isMesh) {
	// 			child.material.map = texture;
	// 		}
	// 	});

	// 	object.position.y = - 95;
	// 	scene.add(object);
	// }

	// const manager = new THREE.LoadingManager(loadModel);
	// manager.onProgress = function (item, loaded, total) {
	// 	console.log(item, loaded, total);
	// };

	// // texture

	// const textureLoader = new THREE.TextureLoader(manager);
	// const texture = textureLoader.load('uv_grid_opengl.jpg');

	// // model

	// const loader = new OBJLoader(manager);
	// // loader.load('male02.obj', (obj) => {
	// loader.load('worldmap.obj', (obj) => {
	// 		obj.scale.set(8,8,8);
	// 		scene.add(obj);
	// 	},
	// 	(xhr) => {
	// 		console.log((xhr.loaded / xhr.total) * 100 + '% loaded')
	// 	},
	// 	(error) => {
	// 		console.log(error)
	// 	}
	// );

	//

	renderer = new THREE.WebGLRenderer();
	renderer.setPixelRatio(window.devicePixelRatio);
	renderer.setSize(window.innerWidth, window.innerHeight);
	container.appendChild(renderer.domElement);

	controls = new OrbitControls(camera, renderer.domElement);
	controls.update();

	// document.addEventListener('mousemove', onDocumentMouseMove);
	window.addEventListener('resize', onWindowResize);
}

function onWindowResize() {
	windowHalfX = window.innerWidth / 2;
	windowHalfY = window.innerHeight / 2;

	camera.aspect = window.innerWidth / window.innerHeight;
	camera.updateProjectionMatrix();

	renderer.setSize(window.innerWidth, window.innerHeight);
}

// function onDocumentMouseMove(event) {
// 	mouseX = (event.clientX - windowHalfX) / 2;
// 	mouseY = (event.clientY - windowHalfY) / 2;
// }

//

function animate() {
	requestAnimationFrame(animate);
	controls.update();
	renderer.render(scene, camera);
	// render();
}

// function render() {
// 	camera.position.x += ( mouseX - camera.position.x) * .05;
// 	camera.position.y += (-mouseY - camera.position.y) * .05;

// 	camera.lookAt(scene.position);
// 	renderer.render(scene, camera);
// }
