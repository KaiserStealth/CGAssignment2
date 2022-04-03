#include <Logging.h>
#include <iostream>

#include <glad/glad.h>
#include <GLFW/glfw3.h>

#include <filesystem>
#include <json.hpp>
#include <fstream>
#include <sstream>
#include <typeindex>
#include <optional>
#include <string>

#include <memory>
#include <ctime>
#include <stdlib.h>

// GLM math library
#include <GLM/glm.hpp>
#include <GLM/gtc/matrix_transform.hpp>
#include <GLM/gtc/type_ptr.hpp>
#define GLM_ENABLE_EXPERIMENTAL
#include <GLM/gtx/common.hpp> // for fmod (floating modulus)

// Graphics
#include "Graphics/IndexBuffer.h"
#include "Graphics/VertexBuffer.h"
#include "Graphics/VertexArrayObject.h"
#include "Graphics/Shader.h"
#include "Graphics/Texture2D.h"
#include "Graphics/TextureCube.h"
#include "Graphics/VertexTypes.h"
#include "Graphics/Font.h"
#include "Graphics/GuiBatcher.h"

// Utilities
#include "Utils/MeshBuilder.h"
#include "Utils/MeshFactory.h"
#include "Utils/ObjLoader.h"
#include "Utils/ImGuiHelper.h"
#include "Utils/ResourceManager/ResourceManager.h"
#include "Utils/FileHelpers.h"
#include "Utils/JsonGlmHelpers.h"
#include "Utils/StringUtils.h"
#include "Utils/GlmDefines.h"

// Gameplay
#include "Gameplay/Material.h"
#include "Gameplay/GameObject.h"
#include "Gameplay/Scene.h"

// Components
#include "Gameplay/Components/IComponent.h"
#include "Gameplay/Components/Camera.h"
#include "Gameplay/Components/RotatingBehaviour.h"
#include "Gameplay/Components/JumpBehaviour.h"
#include "Gameplay/Components/RenderComponent.h"
#include "Gameplay/Components/MaterialSwapBehaviour.h"
#include "Gameplay/Components/CMorphMeshRenderer.h"
#include "Gameplay/Components/CMorphAnimator.h"

// Physics
#include "Gameplay/Physics/RigidBody.h"
#include "Gameplay/Physics/Colliders/BoxCollider.h"
#include "Gameplay/Physics/Colliders/PlaneCollider.h"
#include "Gameplay/Physics/Colliders/SphereCollider.h"
#include "Gameplay/Physics/Colliders/ConvexMeshCollider.h"
#include "Gameplay/Physics/TriggerVolume.h"
#include "Graphics/DebugDraw.h"
#include "Gameplay/Components/TriggerVolumeEnterBehaviour.h"
#include "Gameplay/Components/SimpleCameraControl.h"
#include "Gameplay/Physics/Colliders/CylinderCollider.h"

// GUI
#include "Gameplay/Components/GUI/RectTransform.h"
#include "Gameplay/Components/GUI/GuiPanel.h"
#include "Gameplay/Components/GUI/GuiText.h"
#include "Gameplay/InputEngine.h"

// Animator
#include "NOU/App.h"
#include "NOU/Input.h"
#include "NOU/Entity.h"
#include "NOU/CCamera.h"
#include "NOU/GLTFLoader.h"

//GLTF animator problematic includes
#include "Animation.h"
#include "CAnimator.h"
#include "CSkinnedMeshRenderer.h"
#include "GLTFLoaderSkinning.h"
#include "SkinnedMesh.h"

#include "imgui.h"



//#define LOG_GL_NOTIFICATIONS 

/*
	Handles debug messages from OpenGL
	https://www.khronos.org/opengl/wiki/Debug_Output#Message_Components
	@param source    Which part of OpenGL dispatched the message
	@param type      The type of message (ex: error, performance issues, deprecated behavior)
	@param id        The ID of the error or message (to distinguish between different types of errors, like nullref or index out of range)
	@param severity  The severity of the message (from High to Notification)
	@param length    The length of the message
	@param message   The human readable message from OpenGL
	@param userParam The pointer we set with glDebugMessageCallback (should be the game pointer)
*/
void GlDebugMessage(GLenum source, GLenum type, GLuint id, GLenum severity, GLsizei length, const GLchar* message, const void* userParam) {
	std::string sourceTxt;
	switch (source) {
		case GL_DEBUG_SOURCE_API: sourceTxt = "DEBUG"; break;
		case GL_DEBUG_SOURCE_WINDOW_SYSTEM: sourceTxt = "WINDOW"; break;
		case GL_DEBUG_SOURCE_SHADER_COMPILER: sourceTxt = "SHADER"; break;
		case GL_DEBUG_SOURCE_THIRD_PARTY: sourceTxt = "THIRD PARTY"; break;
		case GL_DEBUG_SOURCE_APPLICATION: sourceTxt = "APP"; break;
		case GL_DEBUG_SOURCE_OTHER: default: sourceTxt = "OTHER"; break;
	}
	switch (severity) {
		case GL_DEBUG_SEVERITY_LOW:          LOG_INFO("[{}] {}", sourceTxt, message); break;
		case GL_DEBUG_SEVERITY_MEDIUM:       LOG_WARN("[{}] {}", sourceTxt, message); break;
		case GL_DEBUG_SEVERITY_HIGH:         LOG_ERROR("[{}] {}", sourceTxt, message); break;
			#ifdef LOG_GL_NOTIFICATIONS
		case GL_DEBUG_SEVERITY_NOTIFICATION: LOG_INFO("[{}] {}", sourceTxt, message); break;
			#endif
		default: break;
	}
}  

// Stores our GLFW window in a global variable for now
GLFWwindow* window;
// The current size of our window in pixels
glm::ivec2 windowSize = glm::ivec2(800, 800);
// The title of our GLFW window
std::string windowTitle = "Union: Vanguard";


// using namespace should generally be avoided, and if used, make sure it's ONLY in cpp files
using namespace Gameplay;
using namespace Gameplay::Physics;

// The scene that we will be rendering
Scene::Sptr scene = nullptr;

void GlfwWindowResizedCallback(GLFWwindow* window, int width, int height) {
	glViewport(0, 0, width, height);
	windowSize = glm::ivec2(width, height);
	if (windowSize.x * windowSize.y > 0) {
		scene->MainCamera->ResizeWindow(width, height);
	}
	GuiBatcher::SetWindowSize({ width, height });
}

/// <summary>
/// Handles intializing GLFW, should be called before initGLAD, but after Logger::Init()
/// Also handles creating the GLFW window
/// </summary>
/// <returns>True if GLFW was initialized, false if otherwise</returns>
bool initGLFW() {
	// Initialize GLFW
	if (glfwInit() == GLFW_FALSE) {
		LOG_ERROR("Failed to initialize GLFW");
		return false;
	}

	//Create a new GLFW window and make it current
	window = glfwCreateWindow(windowSize.x, windowSize.y, windowTitle.c_str(), nullptr, nullptr);
	glfwMakeContextCurrent(window);
	
	// Set our window resized callback
	glfwSetWindowSizeCallback(window, GlfwWindowResizedCallback);

	// Pass the window to the input engine and let it initialize itself
	InputEngine::Init(window);

	GuiBatcher::SetWindowSize(windowSize);

	return true;
}

/// <summary>
/// Handles initializing GLAD and preparing our GLFW window for OpenGL calls
/// </summary>
/// <returns>True if GLAD is loaded, false if there was an error</returns>
bool initGLAD() {
	if (gladLoadGLLoader((GLADloadproc)glfwGetProcAddress) == 0) {
		LOG_ERROR("Failed to initialize Glad");
		return false;
	}
	return true;
}

/// <summary>
/// Draws a widget for saving or loading our scene
/// </summary>
/// <param name="scene">Reference to scene pointer</param>
/// <param name="path">Reference to path string storage</param>
/// <returns>True if a new scene has been loaded</returns>
bool DrawSaveLoadImGui(Scene::Sptr& scene, std::string& path) {
	// Since we can change the internal capacity of an std::string,
	// we can do cool things like this!
	ImGui::InputText("Path", path.data(), path.capacity());

	// Draw a save button, and save when pressed
	if (ImGui::Button("Save")) {
		scene->Save(path);

		std::string newFilename = std::filesystem::path(path).stem().string() + "-manifest.json";
		ResourceManager::SaveManifest(newFilename);
	}
	ImGui::SameLine();
	// Load scene from file button
	if (ImGui::Button("Load")) {
		// Since it's a reference to a ptr, this will
		// overwrite the existing scene!
		scene = nullptr;

		std::string newFilename = std::filesystem::path(path).stem().string() + "-manifest.json";
		ResourceManager::LoadManifest(newFilename);
		scene = Scene::Load(path);

		return true;
	}
	return false;
}

/// <summary>
/// Draws some ImGui controls for the given light
/// </summary>
/// <param name="title">The title for the light's header</param>
/// <param name="light">The light to modify</param>
/// <returns>True if the parameters have changed, false if otherwise</returns>
bool DrawLightImGui(const Scene::Sptr& scene, const char* title, int ix) {
	bool isEdited = false;
	bool result = false;
	Light& light = scene->Lights[ix];
	ImGui::PushID(&light); // We can also use pointers as numbers for unique IDs
	if (ImGui::CollapsingHeader(title)) {
		isEdited |= ImGui::DragFloat3("Pos", &light.Position.x, 0.01f);
		isEdited |= ImGui::ColorEdit3("Col", &light.Color.r);
		isEdited |= ImGui::DragFloat("Range", &light.Range, 0.1f);

		result = ImGui::Button("Delete");
	}
	if (isEdited) {
		scene->SetShaderLight(ix);
	}

	ImGui::PopID();
	return result;
}

/// <summary>
/// Draws a simple window for displaying materials and their editors
/// </summary>
void DrawMaterialsWindow() {
	if (ImGui::Begin("Materials")) {
		ResourceManager::Each<Material>([](Material::Sptr material) {
			material->RenderImGui();
		});
	}
	ImGui::End();
}

/// <summary>
/// handles creating or loading the scene
/// </summary>

int randomizedSkybox = 0;

void CreateScene() {
	bool loadScene = false;  
	// For now we can use a toggle to generate our scene vs load from file
	if (loadScene) {
		ResourceManager::LoadManifest("manifest.json");
		scene = Scene::Load("scene.json");

		// Call scene awake to start up all of our components
		scene->Window = window;
		scene->Awake();
	} 
	else {
		// This time we'll have 2 different shaders, and share data between both of them using the UBO
		// This shader will handle reflective materials 
		Shader::Sptr reflectiveShader = ResourceManager::CreateAsset<Shader>(std::unordered_map<ShaderPartType, std::string>{
			{ ShaderPartType::Vertex, "shaders/vertex_shaders/basic.glsl" },
			{ ShaderPartType::Fragment, "shaders/fragment_shaders/frag_environment_reflective.glsl" }
		});

		// This shader handles our basic materials without reflections (cause they expensive)
		Shader::Sptr basicShader = ResourceManager::CreateAsset<Shader>(std::unordered_map<ShaderPartType, std::string>{
			{ ShaderPartType::Vertex, "shaders/vertex_shaders/basic.glsl" },
			{ ShaderPartType::Fragment, "shaders/fragment_shaders/frag_blinn_phong_textured.glsl" }
		});

		// This shader handles our basic materials without reflections (cause they expensive)
		Shader::Sptr specShader = ResourceManager::CreateAsset<Shader>(std::unordered_map<ShaderPartType, std::string>{
			{ ShaderPartType::Vertex, "shaders/vertex_shaders/basic.glsl" },
			{ ShaderPartType::Fragment, "shaders/fragment_shaders/textured_specular.glsl" }
		});

		// This shader handles our foliage vertex shader example
		Shader::Sptr foliageShader = ResourceManager::CreateAsset<Shader>(std::unordered_map<ShaderPartType, std::string>{
			{ ShaderPartType::Vertex, "shaders/vertex_shaders/foliage.glsl" },
			{ ShaderPartType::Fragment, "shaders/fragment_shaders/screendoor_transparency.glsl" }
		});

		// This shader handles our cel shading example
		Shader::Sptr toonShader = ResourceManager::CreateAsset<Shader>(std::unordered_map<ShaderPartType, std::string>{
			{ ShaderPartType::Vertex, "shaders/vertex_shaders/basic.glsl" },
			{ ShaderPartType::Fragment, "shaders/fragment_shaders/toon_shading.glsl" }
		});


		///////////////////// NEW SHADERS ////////////////////////////////////////////

		// This shader handles our displacement mapping example
		Shader::Sptr displacementShader = ResourceManager::CreateAsset<Shader>(std::unordered_map<ShaderPartType, std::string>{
			{ ShaderPartType::Vertex, "shaders/vertex_shaders/displacement_mapping.glsl" },
			{ ShaderPartType::Fragment, "shaders/fragment_shaders/frag_tangentspace_normal_maps.glsl" }
		});

		// This shader handles our displacement mapping example
		Shader::Sptr tangentSpaceMapping = ResourceManager::CreateAsset<Shader>(std::unordered_map<ShaderPartType, std::string>{
			{ ShaderPartType::Vertex, "shaders/vertex_shaders/basic.glsl" },
			{ ShaderPartType::Fragment, "shaders/fragment_shaders/frag_tangentspace_normal_maps.glsl" }
		});

		// This shader handles our multitexturing example
		Shader::Sptr multiTextureShader = ResourceManager::CreateAsset<Shader>(std::unordered_map<ShaderPartType, std::string>{
			{ ShaderPartType::Vertex, "shaders/vertex_shaders/vert_multitextured.glsl" },
			{ ShaderPartType::Fragment, "shaders/fragment_shaders/frag_multitextured.glsl" }
		});

		// Load in the meshes
		MeshResource::Sptr monkeyMesh = ResourceManager::CreateAsset<MeshResource>("Monkey.obj");
		MeshResource::Sptr towerGardenMesh = ResourceManager::CreateAsset<MeshResource>("FinalArea.obj");
		MeshResource::Sptr towerCannonMesh = ResourceManager::CreateAsset<MeshResource>("TowerV1.obj");
		MeshResource::Sptr cannonBallMesh = ResourceManager::CreateAsset<MeshResource>("Cannonball.obj");
		MeshResource::Sptr goblinMesh = ResourceManager::CreateAsset<MeshResource>("goblinfullrig.obj");
		MeshResource::Sptr spearMesh = ResourceManager::CreateAsset<MeshResource>("CubeTester.fbx");

		// Load in some textures
		Texture2D::Sptr    boxTexture = ResourceManager::CreateAsset<Texture2D>("textures/box-diffuse.png");
		Texture2D::Sptr    boxSpec = ResourceManager::CreateAsset<Texture2D>("textures/box-specular.png");
		Texture2D::Sptr    monkeyTex = ResourceManager::CreateAsset<Texture2D>("textures/monkey-uvMap.png");
		Texture2D::Sptr    gardenTowerTexture = ResourceManager::CreateAsset<Texture2D>("textures/YYY5.png");
		Texture2D::Sptr    goblinTex = ResourceManager::CreateAsset<Texture2D>("textures/red.png");
		Texture2D::Sptr    leafTex = ResourceManager::CreateAsset<Texture2D>("textures/leaves.png");
		leafTex->SetMinFilter(MinFilter::Nearest);
		leafTex->SetMagFilter(MagFilter::Nearest);


		// Here we'll load in the cubemap, as well as a special shader to handle drawing the skybox
		TextureCube::Sptr sampleCubemap = ResourceManager::CreateAsset<TextureCube>("cubemaps/sample/sample.jpg");
		Shader::Sptr      sampleSkyboxShader = ResourceManager::CreateAsset<Shader>(std::unordered_map<ShaderPartType, std::string>{
			{ ShaderPartType::Vertex, "shaders/vertex_shaders/skybox_vert.glsl" },
			{ ShaderPartType::Fragment, "shaders/fragment_shaders/skybox_frag.glsl" }
		});

		TextureCube::Sptr oceanCubemap = ResourceManager::CreateAsset<TextureCube>("cubemaps/ocean/ocean.jpg");
		Shader::Sptr      oceanSkyboxShader = ResourceManager::CreateAsset<Shader>(std::unordered_map<ShaderPartType, std::string>{
			{ ShaderPartType::Vertex, "shaders/vertex_shaders/skybox_vert.glsl" },
			{ ShaderPartType::Fragment, "shaders/fragment_shaders/skybox_frag.glsl" }
		});

		TextureCube::Sptr clearDayCubemap = ResourceManager::CreateAsset<TextureCube>("cubemaps/clearDay/clearDay.jpg");
		Shader::Sptr      clearDaySkyboxShader = ResourceManager::CreateAsset<Shader>(std::unordered_map<ShaderPartType, std::string>{
			{ ShaderPartType::Vertex, "shaders/vertex_shaders/skybox_vert.glsl" },
			{ ShaderPartType::Fragment, "shaders/fragment_shaders/skybox_frag.glsl" }
		});

		TextureCube::Sptr clearMorningCubemap = ResourceManager::CreateAsset<TextureCube>("cubemaps/clearMorning/clearMorning.jpg");
		Shader::Sptr      clearMorningSkyboxShader = ResourceManager::CreateAsset<Shader>(std::unordered_map<ShaderPartType, std::string>{
			{ ShaderPartType::Vertex, "shaders/vertex_shaders/skybox_vert.glsl" },
			{ ShaderPartType::Fragment, "shaders/fragment_shaders/skybox_frag.glsl" }
		});

		TextureCube::Sptr clearNightCubemap = ResourceManager::CreateAsset<TextureCube>("cubemaps/clearNight/clearNight.jpg");
		Shader::Sptr      clearNightSkyboxShader = ResourceManager::CreateAsset<Shader>(std::unordered_map<ShaderPartType, std::string>{
			{ ShaderPartType::Vertex, "shaders/vertex_shaders/skybox_vert.glsl" },
			{ ShaderPartType::Fragment, "shaders/fragment_shaders/skybox_frag.glsl" }
		});
		

		/*TextureCube::Sptr morshuCubemap = ResourceManager::CreateAsset<TextureCube>("cubemaps/morshuSky/morshu.jpg");
		Shader::Sptr      morshuSkyboxShader = ResourceManager::CreateAsset<Shader>(std::unordered_map<ShaderPartType, std::string>{
			{ ShaderPartType::Vertex, "shaders/vertex_shaders/skybox_vert.glsl" },
			{ ShaderPartType::Fragment, "shaders/fragment_shaders/skybox_frag.glsl" }
		});*/

		TextureCube::Sptr settingCubemap = ResourceManager::CreateAsset<TextureCube>("cubemaps/setting/setting.jpg");
		Shader::Sptr      settingSkyboxShader = ResourceManager::CreateAsset<Shader>(std::unordered_map<ShaderPartType, std::string>{
			{ ShaderPartType::Vertex, "shaders/vertex_shaders/skybox_vert.glsl" },
			{ ShaderPartType::Fragment, "shaders/fragment_shaders/skybox_frag.glsl" }
		});

		// Create an empty scene
		scene = std::make_shared<Scene>();

		// Setting up our enviroment map
		 randomizedSkybox = rand() % 3 + 1;
		std::cout << randomizedSkybox << std::endl;

		switch (randomizedSkybox) {
		case 1:
			scene->SetSkyboxTexture(clearDayCubemap);
			break;
		case 2:
			scene->SetSkyboxTexture(clearMorningCubemap);
			break;
		case 3:
			scene->SetSkyboxTexture(clearNightCubemap);
			break;
		}

		//scene->SetSkyboxTexture(clearMorningCubemap);
		scene->SetSkyboxShader(sampleSkyboxShader);
		// Since the skybox I used was for Y-up, we need to rotate it 90 deg around the X-axis to convert it to z-up
		scene->SetSkyboxRotation(glm::rotate(MAT4_IDENTITY, glm::half_pi<float>(), glm::vec3(1.0f, 0.0f, 0.0f)));

		// Create our materials
		// This will be our box material, with no environment reflections
		Material::Sptr boxMaterial = ResourceManager::CreateAsset<Material>(basicShader);
		{
			boxMaterial->Name = "Box";
			boxMaterial->Set("u_Material.Diffuse", boxTexture);
			boxMaterial->Set("u_Material.Shininess", 0.1f);
		}

		// This will be the reflective material, we'll make the whole thing 90% reflective
		Material::Sptr monkeyMaterial = ResourceManager::CreateAsset<Material>(reflectiveShader);
		{
			monkeyMaterial->Name = "Monkey";
			monkeyMaterial->Set("u_Material.Diffuse", monkeyTex);
			monkeyMaterial->Set("u_Material.Shininess", 0.5f);
		}

		// This will be the reflective material, we'll make the whole thing 90% reflective
		Material::Sptr testMaterial = ResourceManager::CreateAsset<Material>(specShader);
		{
			testMaterial->Name = "Box-Specular";
			testMaterial->Set("u_Material.Diffuse", boxTexture);
			testMaterial->Set("u_Material.Specular", boxSpec);
		}

		Material::Sptr gardenTowerMaterial = ResourceManager::CreateAsset<Material>(basicShader);
		{
			gardenTowerMaterial->Name = "GardenTowerMat";
			gardenTowerMaterial->Set("u_Material.Diffuse", gardenTowerTexture);
			gardenTowerMaterial->Set("u_Material.Shininess", 0.1f);
		}

		Material::Sptr cannonBallMaterial = ResourceManager::CreateAsset<Material>(basicShader);
		{
			cannonBallMaterial->Name = "CannonBallMat";
			cannonBallMaterial->Set("u_Material.Diffuse", gardenTowerTexture);
			cannonBallMaterial->Set("u_Material.Shininess", 0.1f);
		}

		Material::Sptr goblinMaterial = ResourceManager::CreateAsset<Material>(reflectiveShader);
		{
			goblinMaterial->Name = "Goblin";
			goblinMaterial->Set("u_Material.Diffuse", goblinTex);
			goblinMaterial->Set("u_Material.Shininess", 0.1f);
		}

		// Our foliage vertex shader material
		Material::Sptr foliageMaterial = ResourceManager::CreateAsset<Material>(foliageShader);
		{
			foliageMaterial->Name = "Foliage Shader";
			foliageMaterial->Set("u_Material.Diffuse", leafTex);
			foliageMaterial->Set("u_Material.Shininess", 0.1f);
			foliageMaterial->Set("u_Material.Threshold", 0.1f);

			foliageMaterial->Set("u_WindDirection", glm::vec3(1.0f, 1.0f, 0.0f));
			foliageMaterial->Set("u_WindStrength", 0.5f);
			foliageMaterial->Set("u_VerticalScale", 1.0f);
			foliageMaterial->Set("u_WindSpeed", 1.0f);
		}

		// Our toon shader material
		Material::Sptr toonMaterial = ResourceManager::CreateAsset<Material>(toonShader);
		{
			toonMaterial->Name = "Toon";
			toonMaterial->Set("u_Material.Diffuse", boxTexture);
			toonMaterial->Set("u_Material.Shininess", 0.1f);
			toonMaterial->Set("u_Material.Steps", 8);
		}

		/////////////// NEW MATERIALS ////////////////////

		Material::Sptr displacementTest = ResourceManager::CreateAsset<Material>(displacementShader);
		{
			Texture2D::Sptr displacementMap = ResourceManager::CreateAsset<Texture2D>("textures/displacement_map.png");
			Texture2D::Sptr normalMap = ResourceManager::CreateAsset<Texture2D>("textures/normal_map.png");
			Texture2D::Sptr diffuseMap = ResourceManager::CreateAsset<Texture2D>("textures/bricks_diffuse.png");

			displacementTest->Name = "Displacement Map";
			displacementTest->Set("u_Material.Diffuse", diffuseMap);
			displacementTest->Set("s_Heightmap", displacementMap);
			displacementTest->Set("s_NormalMap", normalMap);
			displacementTest->Set("u_Material.Shininess", 0.5f);
			displacementTest->Set("u_Scale", 0.1f);
		}

		Material::Sptr normalmapMat = ResourceManager::CreateAsset<Material>(tangentSpaceMapping);
		{
			Texture2D::Sptr normalMap = ResourceManager::CreateAsset<Texture2D>("textures/normal_map.png");
			Texture2D::Sptr diffuseMap = ResourceManager::CreateAsset<Texture2D>("textures/bricks_diffuse.png");

			normalmapMat->Name = "Tangent Space Normal Map";
			normalmapMat->Set("u_Material.Diffuse", diffuseMap);
			normalmapMat->Set("s_NormalMap", normalMap);
			normalmapMat->Set("u_Material.Shininess", 0.5f);
			normalmapMat->Set("u_Scale", 0.1f);
		}

		Material::Sptr multiTextureMat = ResourceManager::CreateAsset<Material>(multiTextureShader);
		{
			Texture2D::Sptr sand = ResourceManager::CreateAsset<Texture2D>("textures/terrain/sand.png");
			Texture2D::Sptr grass = ResourceManager::CreateAsset<Texture2D>("textures/terrain/grass.png");

			multiTextureMat->Name = "Multitexturing";
			multiTextureMat->Set("u_Material.DiffuseA", sand);
			multiTextureMat->Set("u_Material.DiffuseB", grass);
			multiTextureMat->Set("u_Material.Shininess", 0.5f);
			multiTextureMat->Set("u_Scale", 0.1f);
		}

		// Create some lights for our scene
		scene->Lights.resize(5);
		scene->Lights[0].Position = glm::vec3(15.0f, -10.0f, 12.0f);
		scene->Lights[0].Color = glm::vec3(1.0f, 1.0f, 1.0f);
		scene->Lights[0].Range = 100.0f;

		scene->Lights[1].Position = glm::vec3(10.0f, 20.0f, 12.0f);
		scene->Lights[1].Color = glm::vec3(1.0f, 1.0f, 1.0f);
		scene->Lights[1].Range = 100.0f;

		scene->Lights[2].Position = glm::vec3(40.0f, -10.0f, 12.0f);
		scene->Lights[2].Color = glm::vec3(1.0f, 1.0f, 1.0f);
		scene->Lights[2].Range = 100.0f;

		scene->Lights[3].Position = glm::vec3(12.0f, -40.0f, 12.0f);
		scene->Lights[3].Color = glm::vec3(1.0f, 1.0f, 1.0f);
		scene->Lights[3].Range = 100.0f;

		scene->Lights[4].Position = glm::vec3(-15.0f, -10.0f, 12.0f);
		scene->Lights[4].Color = glm::vec3(1.0f, 1.0f, 1.0f);
		scene->Lights[4].Range = 100.0f;


		// We'll create a mesh that is a simple plane that we can resize later
		MeshResource::Sptr planeMesh = ResourceManager::CreateAsset<MeshResource>();
		planeMesh->AddParam(MeshBuilderParam::CreatePlane(ZERO, UNIT_Z, UNIT_X, glm::vec2(1.0f)));
		planeMesh->GenerateMesh();

		MeshResource::Sptr sphere = ResourceManager::CreateAsset<MeshResource>();
		sphere->AddParam(MeshBuilderParam::CreateIcoSphere(ZERO, ONE, 5));
		sphere->GenerateMesh();

		// Set up the scene's camera
		GameObject::Sptr camera = scene->CreateGameObject("Main Camera");
		{
			camera->SetPostion(glm::vec3(12.760f, -10.420f, 6.0f));
			camera->SetRotation(glm::vec3(90.0f, 0.0f, 0.0f));

			camera->Add<SimpleCameraControl>();

			Camera::Sptr cam = camera->Add<Camera>();
			// Make sure that the camera is set as the scene's main camera!
			scene->MainCamera = cam;
		}

		// Set up all our sample objects
		/*GameObject::Sptr plane = scene->CreateGameObject("Plane");
		{
			// Make a big tiled mesh
			MeshResource::Sptr tiledMesh = ResourceManager::CreateAsset<MeshResource>();
			tiledMesh->AddParam(MeshBuilderParam::CreatePlane(ZERO, UNIT_Z, UNIT_X, glm::vec2(100.0f), glm::vec2(20.0f)));
			tiledMesh->GenerateMesh();

			// Create and attach a RenderComponent to the object to draw our mesh
			RenderComponent::Sptr renderer = plane->Add<RenderComponent>();
			renderer->SetMesh(tiledMesh);
			renderer->SetMaterial(boxMaterial);

			// Attach a plane collider that extends infinitely along the X/Y axis
			RigidBody::Sptr physics = plane->Add<RigidBody>(/*static by default);
			physics->AddCollider(BoxCollider::Create(glm::vec3(50.0f, 50.0f, 1.0f)))->SetPosition({ 0,0,-1 });
		}*/

		GameObject::Sptr towerGarden = scene->CreateGameObject("towerGarden");
		{
			// Set position in the scene
			towerGarden->SetPostion(glm::vec3(-118.0f, -154.0f, -4.0f));
			towerGarden->SetRotation(glm::vec3(90.0f, 0.0f, 0.0f));

			// Add some behaviour that relies on the physics body
			//towerGarden->Add<JumpBehaviour>();

			// Create and attach a renderer for the monkey
			RenderComponent::Sptr renderer = towerGarden->Add<RenderComponent>();
			renderer->SetMesh(towerGardenMesh);
			renderer->SetMaterial(gardenTowerMaterial);

			// Add a dynamic rigid body to this monkey
			//RigidBody::Sptr physics = full1->Add<RigidBody>(RigidBodyType::Dynamic);
			//physics->AddCollider(ConvexMeshCollider::Create());
		}

		GameObject::Sptr cannonBall = scene->CreateGameObject("cannonBall");
		{
			cannonBall->SetPostion(glm::vec3(12.6f, -10.4f, 1.0f));
			cannonBall->SetRotation(glm::vec3(90.0f, 0.0f, 0.0f));
			cannonBall->SetScale(glm::vec3(1.f));

			//Add a rigidbody to hit with force
			RigidBody::Sptr ballPhy = cannonBall->Add<RigidBody>(RigidBodyType::Dynamic);
			ballPhy->SetMass(5.0f);

			// Create and attach a renderer for the monkey
			RenderComponent::Sptr renderer = cannonBall->Add<RenderComponent>();
			renderer->SetMesh(cannonBallMesh);
			renderer->SetMaterial(cannonBallMaterial);
		}

		GameObject::Sptr towerCannon = scene->CreateGameObject("towerCannon");
		{
			towerCannon->SetPostion(glm::vec3(12.6f, -10.4f, 1.0f));
			towerCannon->SetRotation(glm::vec3(90.0f, 0.0f, 0.0f));

			// Add some behaviour that relies on the physics body
			//towerGarden->Add<JumpBehaviour>();

			// Create and attach a renderer for the monkey
			RenderComponent::Sptr renderer = towerCannon->Add<RenderComponent>();
			renderer->SetMesh(towerCannonMesh);
			renderer->SetMaterial(gardenTowerMaterial);

		}

		GameObject::Sptr towerSpears = scene->CreateGameObject("towerSpears");
		{
			towerSpears->SetPostion(glm::vec3(12.6f, -10.4f, 1.0f));
			towerSpears->SetRotation(glm::vec3(90.0f, 0.0f, 0.0f));

			// Add some behaviour that relies on the physics body
			//towerGarden->Add<JumpBehaviour>();

			// Create and attach a renderer for the monkey
			RenderComponent::Sptr renderer = towerSpears->Add<RenderComponent>();
			renderer->SetMesh(spearMesh);
			renderer->SetMaterial(goblinMaterial);

		}


		GameObject::Sptr goblin1 = scene->CreateGameObject("goblin1");
		{
			// Set position in the scene
			goblin1->SetPostion(glm::vec3(12.760f, 0.0f, 1.0f));
			goblin1->SetRotation(glm::vec3(90.0f, 0.0f, -90.0f));
			goblin1->SetScale(glm::vec3(0.7f));

			// Add some behaviour that relies on the physics body
			//towerGarden->Add<JumpBehaviour>();

			// Create and attach a renderer for the monkey
			RenderComponent::Sptr renderer = goblin1->Add<RenderComponent>();
			renderer->SetMesh(goblinMesh);
			renderer->SetMaterial(goblinMaterial);


			// Add a dynamic rigid body to this monkey
			//RigidBody::Sptr physics = full1->Add<RigidBody>(RigidBodyType::Dynamic);
			//physics->AddCollider(ConvexMeshCollider::Create());
		}

		GameObject::Sptr GLTFTest = scene->CreateGameObject("GLTF Test");
		{
			GLTFTest->SetPostion(glm::vec3(0.0f, 0.0f, 0.0f));
			GLTFTest->SetRotation(glm::vec3(0.0f, 0.0f, 0.0f));
			GLTFTest->SetScale(glm::vec3(1.0f));
			
			//GLTFTest->Add<nou::CAnimator>();
			
			
		}
		//GameObject::Sptr demoBase = scene->CreateGameObject("Demo Parent");

		// Create a trigger volume for testing how we can detect collisions with objects!
		/*GameObject::Sptr trigger = scene->CreateGameObject("Trigger");
		{
			TriggerVolume::Sptr volume = trigger->Add<TriggerVolume>();
			CylinderCollider::Sptr collider = CylinderCollider::Create(glm::vec3(3.0f, 3.0f, 1.0f));
			collider->SetPosition(glm::vec3(0.0f, 0.0f, 0.5f));
			volume->AddCollider(collider);

			trigger->Add<TriggerVolumeEnterBehaviour>();
		}*/

		/////////////////////////// UI //////////////////////////////
		GameObject::Sptr canvas = scene->CreateGameObject("Main Menu");
		{
			RectTransform::Sptr transform = canvas->Add<RectTransform>();
			transform->SetMin({ 100, 100 });
			transform->SetMax({ 700, 800 });
			transform->SetPosition(glm::vec2(400.0f, 400.0f));
			
			GuiPanel::Sptr canPanel = canvas->Add<GuiPanel>();
			canPanel->SetColor(glm::vec4(0.6f, 0.3f, 0.0f, 1.0f));
			
			GameObject::Sptr subPanel = scene->CreateGameObject("Button1");
			{
				RectTransform::Sptr transform = subPanel->Add<RectTransform>();
				transform->SetMin({ 10, 10 });
				transform->SetMax({ 590, 128 });
				transform->SetPosition(glm::vec2(300.0f, 300.0f));

				GuiPanel::Sptr panel = subPanel->Add<GuiPanel>();
				//panel->SetTexture(ResourceManager::CreateAsset<Texture2D>("textures/PlayIdle.png"));				
			    panel->SetColor(glm::vec4(0.3f, 0.15f, 0.0f, 1.0f));

				Font::Sptr font = ResourceManager::CreateAsset<Font>("fonts/Roboto-Medium.ttf", 36.0f);
				font->Bake();

				GuiText::Sptr text = subPanel->Add<GuiText>();
				text->SetText("Play");
				text->SetFont(font);
				
			}
			canvas->AddChild(subPanel);

			GameObject::Sptr subPanel2 = scene->CreateGameObject("Button2");
			{
				RectTransform::Sptr transform = subPanel2->Add<RectTransform>();
				transform->SetMin({ 10, 10 });
				transform->SetMax({ 590, 128 });
				transform->SetPosition(glm::vec2(300.0f, 450.0f));

				GuiPanel::Sptr panel = subPanel2->Add<GuiPanel>();
				panel->SetColor(glm::vec4(0.3f, 0.15f, 0.0f, 1.0f));

				Font::Sptr font = ResourceManager::CreateAsset<Font>("fonts/Roboto-Medium.ttf", 36.0f);
				font->Bake();

				GuiText::Sptr text = subPanel2->Add<GuiText>();
				text->SetText("Settings");
				text->SetFont(font);

			}
			canvas->AddChild(subPanel2);

			GameObject::Sptr subPanel3 = scene->CreateGameObject("Button3");
			{
				RectTransform::Sptr transform = subPanel3->Add<RectTransform>();
				transform->SetMin({ 10, 10 });
				transform->SetMax({ 590, 128 });
				transform->SetPosition(glm::vec2(300.0f, 600.0f));

				GuiPanel::Sptr panel = subPanel3->Add<GuiPanel>();
				panel->SetColor(glm::vec4(0.3f, 0.15f, 0.0f, 1.0f));

				Font::Sptr font = ResourceManager::CreateAsset<Font>("fonts/Roboto-Medium.ttf", 36.0f);
				font->Bake();

				GuiText::Sptr text = subPanel3->Add<GuiText>();
				text->SetText("Exit");
				text->SetFont(font);

			}
			canvas->AddChild(subPanel3);

			GameObject::Sptr subPanel4 = scene->CreateGameObject("Title");
			{
				RectTransform::Sptr transform = subPanel4->Add<RectTransform>();
				transform->SetMin({ 10, 10 });
				transform->SetMax({ 590, 128 });
				transform->SetPosition(glm::vec2(300.0f, 100.0f));

				GuiPanel::Sptr panel = subPanel4->Add<GuiPanel>();
				panel->SetColor(glm::vec4(1.0f, 1.0f, 0.0f, 1.0f));

				Font::Sptr font = ResourceManager::CreateAsset<Font>("fonts/Roboto-Medium.ttf", 36.0f);
				font->Bake();

				GuiText::Sptr text = subPanel4->Add<GuiText>();
				text->SetText("Vanguard");
				text->SetFont(font);

			}
			canvas->AddChild(subPanel4);
		}

		GameObject::Sptr canvas2 = scene->CreateGameObject("Settings Menu");
		{
			RectTransform::Sptr transform = canvas2->Add<RectTransform>();
			transform->SetMin({ 100, 100 });
			transform->SetMax({ 700, 800 });
			transform->SetPosition(glm::vec2(400.0f, 400.0f));

			GuiPanel::Sptr canPanel = canvas2->Add<GuiPanel>();
			canPanel->SetColor(glm::vec4(0.6f, 0.3f, 0.0f, 1.0f));

			GameObject::Sptr subPanel = scene->CreateGameObject("Button4");
			{
				RectTransform::Sptr transform = subPanel->Add<RectTransform>();
				transform->SetMin({ 10, 10 });
				transform->SetMax({ 590, 128 });
				transform->SetPosition(glm::vec2(300.0f, 300.0f));

				GuiPanel::Sptr panel = subPanel->Add<GuiPanel>();
				panel->SetColor(glm::vec4(0.3f, 0.15f, 0.0f, 1.0f));

				Font::Sptr font = ResourceManager::CreateAsset<Font>("fonts/Roboto-Medium.ttf", 36.0f);
				font->Bake();

				GuiText::Sptr text = subPanel->Add<GuiText>();
				text->SetText("Settings stuff");
				text->SetFont(font);

			}
			canvas2->AddChild(subPanel);

			GameObject::Sptr subPanel2 = scene->CreateGameObject("Button5");
			{
				RectTransform::Sptr transform = subPanel2->Add<RectTransform>();
				transform->SetMin({ 10, 10 });
				transform->SetMax({ 590, 128 });
				transform->SetPosition(glm::vec2(300.0f, 450.0f));

				GuiPanel::Sptr panel = subPanel2->Add<GuiPanel>();
				panel->SetColor(glm::vec4(0.3f, 0.15f, 0.0f, 1.0f));

				Font::Sptr font = ResourceManager::CreateAsset<Font>("fonts/Roboto-Medium.ttf", 36.0f);
				font->Bake();

				GuiText::Sptr text = subPanel2->Add<GuiText>();
				text->SetText("More Settings");
				text->SetFont(font);

			}
			canvas2->AddChild(subPanel2);

			GameObject::Sptr subPanel3 = scene->CreateGameObject("Button6");
			{
				RectTransform::Sptr transform = subPanel3->Add<RectTransform>();
				transform->SetMin({ 10, 10 });
				transform->SetMax({ 590, 128 });
				transform->SetPosition(glm::vec2(300.0f, 600.0f));

				GuiPanel::Sptr panel = subPanel3->Add<GuiPanel>();
				panel->SetColor(glm::vec4(0.3f, 0.15f, 0.0f, 1.0f));

				Font::Sptr font = ResourceManager::CreateAsset<Font>("fonts/Roboto-Medium.ttf", 36.0f);
				font->Bake();

				GuiText::Sptr text = subPanel3->Add<GuiText>();
				text->SetText("Back");
				text->SetFont(font);

			}
			canvas2->AddChild(subPanel3);

			GameObject::Sptr subPanel4 = scene->CreateGameObject("Settings Title");
			{
				RectTransform::Sptr transform = subPanel4->Add<RectTransform>();
				transform->SetMin({ 10, 10 });
				transform->SetMax({ 590, 128 });
				transform->SetPosition(glm::vec2(300.0f, 100.0f));

				GuiPanel::Sptr panel = subPanel4->Add<GuiPanel>();
				panel->SetColor(glm::vec4(1.0f, 1.0f, 0.0f, 1.0f));

				Font::Sptr font = ResourceManager::CreateAsset<Font>("fonts/Roboto-Medium.ttf", 36.0f);
				font->Bake();

				GuiText::Sptr text = subPanel4->Add<GuiText>();
				text->SetText("Settings");
				text->SetFont(font);

			}
			canvas2->AddChild(subPanel4);
		}

		GameObject::Sptr canvas3 = scene->CreateGameObject("inGameGUI");
		{
			GameObject::Sptr subPanel1 = scene->CreateGameObject("Score");
			{
				RectTransform::Sptr transform = subPanel1->Add<RectTransform>();
				transform->SetMin({ 6, 10 });
				transform->SetMax({ 110, 50 });
				transform->SetPosition(glm::vec2(70.0f, 775.0f));

				GuiPanel::Sptr canPanel = subPanel1->Add<GuiPanel>();
				canPanel->SetColor(glm::vec4(0.6f, 0.3f, 0.0f, 1.0f));

				Font::Sptr font = ResourceManager::CreateAsset<Font>("fonts/Roboto-Medium.ttf", 16.0f);
				font->Bake();

				GuiText::Sptr text = subPanel1->Add<GuiText>();
				text->SetText("0");
				text->SetFont(font);
			}
			canvas3->AddChild(subPanel1);

			GameObject::Sptr subPanel2 = scene->CreateGameObject("Power Bar");
			{
				RectTransform::Sptr transform = subPanel2->Add<RectTransform>();
				transform->SetMin({ 6, 10 });
				transform->SetMax({ 180, 50 });
				transform->SetPosition(glm::vec2(700.0f, 775.0f));

				GuiPanel::Sptr panel = subPanel2->Add<GuiPanel>();
				panel->SetColor(glm::vec4(0.6f, 0.3f, 0.0f, 1.0f));

				Font::Sptr font = ResourceManager::CreateAsset<Font>("fonts/Roboto-Medium.ttf", 16.0f);
				font->Bake();

				GuiText::Sptr text = subPanel2->Add<GuiText>();
				text->SetText("Power");
				text->SetFont(font);

			}
			canvas3->AddChild(subPanel2);

			GameObject::Sptr subPanel3 = scene->CreateGameObject("Charge Level");
			{
				RectTransform::Sptr transform = subPanel3->Add<RectTransform>();
				transform->SetMin({ 0, 10 });
				transform->SetMax({ 10, 20 });
				transform->SetPosition(glm::vec2(630.0f, 780.0f));

				GuiPanel::Sptr panel = subPanel3->Add<GuiPanel>();
				panel->SetColor(glm::vec4(0.0f, 1.0f, 0.0f, 1.0f));
				panel->SetTexture(ResourceManager::CreateAsset<Texture2D>("textures/red.png"));
			}
			canvas3->AddChild(subPanel3);

			GameObject::Sptr subPanel4 = scene->CreateGameObject("Health Bar");
			{
				RectTransform::Sptr transform = subPanel4->Add<RectTransform>();
				transform->SetMin({ 6, 10 });
				transform->SetMax({ 180, 50 });
				transform->SetPosition(glm::vec2(100.0f, 30.0f));

				GuiPanel::Sptr panel = subPanel4->Add<GuiPanel>();
				panel->SetColor(glm::vec4(0.6f, 0.3f, 0.0f, 1.0f));

				Font::Sptr font = ResourceManager::CreateAsset<Font>("fonts/Roboto-Medium.ttf", 16.0f);
				font->Bake();

				GuiText::Sptr text = subPanel4->Add<GuiText>();
				text->SetText("Tower Health");
				text->SetFont(font);

			}
			canvas3->AddChild(subPanel4);

			GameObject::Sptr subPanel5 = scene->CreateGameObject("Health Level");
			{
				RectTransform::Sptr transform = subPanel5->Add<RectTransform>();
				transform->SetMin({ 0, 10 });
				transform->SetMax({ 150, 20 });
				transform->SetPosition(glm::vec2(100.0f, 35.0f));

				GuiPanel::Sptr panel = subPanel5->Add<GuiPanel>();
				panel->SetColor(glm::vec4(0.0f, 1.0f, 0.0f, 1.0f));
				panel->SetTexture(ResourceManager::CreateAsset<Texture2D>("textures/red.png"));
				
			}
			canvas3->AddChild(subPanel5);
		}

		GameObject::Sptr canvas4 = scene->CreateGameObject("Pause Menu");
		{
			RectTransform::Sptr transform = canvas4->Add<RectTransform>();
			transform->SetMin({ 100, 100 });
			transform->SetMax({ 700, 800 });
			transform->SetPosition(glm::vec2(400.0f, 400.0f));

			GuiPanel::Sptr canPanel = canvas4->Add<GuiPanel>();
			canPanel->SetColor(glm::vec4(0.6f, 0.3f, 0.0f, 1.0f));

			GameObject::Sptr subPanel2 = scene->CreateGameObject("Button7");
			{
				RectTransform::Sptr transform = subPanel2->Add<RectTransform>();
				transform->SetMin({ 10, 10 });
				transform->SetMax({ 590, 128 });
				transform->SetPosition(glm::vec2(300.0f, 450.0f));

				GuiPanel::Sptr panel = subPanel2->Add<GuiPanel>();
				panel->SetColor(glm::vec4(0.3f, 0.15f, 0.0f, 1.0f));

				Font::Sptr font = ResourceManager::CreateAsset<Font>("fonts/Roboto-Medium.ttf", 36.0f);
				font->Bake();

				GuiText::Sptr text = subPanel2->Add<GuiText>();
				text->SetText("Exit Game");
				text->SetFont(font);

			}
			canvas4->AddChild(subPanel2);

			GameObject::Sptr subPanel3 = scene->CreateGameObject("Button8");
			{
				RectTransform::Sptr transform = subPanel3->Add<RectTransform>();
				transform->SetMin({ 10, 10 });
				transform->SetMax({ 590, 128 });
				transform->SetPosition(glm::vec2(300.0f, 600.0f));

				GuiPanel::Sptr panel = subPanel3->Add<GuiPanel>();
				panel->SetColor(glm::vec4(0.3f, 0.15f, 0.0f, 1.0f));

				Font::Sptr font = ResourceManager::CreateAsset<Font>("fonts/Roboto-Medium.ttf", 36.0f);
				font->Bake();

				GuiText::Sptr text = subPanel3->Add<GuiText>();
				text->SetText("Resume");
				text->SetFont(font);

			}
			canvas4->AddChild(subPanel3);

			GameObject::Sptr subPanel4 = scene->CreateGameObject("Paused Title");
			{
				RectTransform::Sptr transform = subPanel4->Add<RectTransform>();
				transform->SetMin({ 10, 10 });
				transform->SetMax({ 590, 128 });
				transform->SetPosition(glm::vec2(300.0f, 100.0f));

				GuiPanel::Sptr panel = subPanel4->Add<GuiPanel>();
				panel->SetColor(glm::vec4(1.0f, 1.0f, 0.0f, 1.0f));

				Font::Sptr font = ResourceManager::CreateAsset<Font>("fonts/Roboto-Medium.ttf", 36.0f);
				font->Bake();

				GuiText::Sptr text = subPanel4->Add<GuiText>();
				text->SetText("Paused");
				text->SetFont(font);

			}
			canvas4->AddChild(subPanel4);
		}

		GameObject::Sptr canvas5 = scene->CreateGameObject("Win");
		{
			RectTransform::Sptr transform = canvas5->Add<RectTransform>();
			transform->SetMin({ 100, 100 });
			transform->SetMax({ 700, 800 });
			transform->SetPosition(glm::vec2(400.0f, 400.0f));

			GuiPanel::Sptr canPanel = canvas5->Add<GuiPanel>();
			canPanel->SetColor(glm::vec4(0.6f, 0.3f, 0.0f, 1.0f));

			GameObject::Sptr subPanel2 = scene->CreateGameObject("FinalScoreW");
			{
				RectTransform::Sptr transform = subPanel2->Add<RectTransform>();
				transform->SetMin({ 10, 10 });
				transform->SetMax({ 590, 128 });
				transform->SetPosition(glm::vec2(300.0f, 450.0f));

				GuiPanel::Sptr panel = subPanel2->Add<GuiPanel>();
				panel->SetColor(glm::vec4(0.3f, 0.15f, 0.0f, 1.0f));

				Font::Sptr font = ResourceManager::CreateAsset<Font>("fonts/Roboto-Medium.ttf", 36.0f);
				font->Bake();

				GuiText::Sptr text = subPanel2->Add<GuiText>();
				text->SetText("0");
				text->SetFont(font);

			}
			canvas5->AddChild(subPanel2);

			GameObject::Sptr subPanel3 = scene->CreateGameObject("Button9");
			{
				RectTransform::Sptr transform = subPanel3->Add<RectTransform>();
				transform->SetMin({ 10, 10 });
				transform->SetMax({ 590, 128 });
				transform->SetPosition(glm::vec2(300.0f, 600.0f));

				GuiPanel::Sptr panel = subPanel3->Add<GuiPanel>();
				panel->SetColor(glm::vec4(0.3f, 0.15f, 0.0f, 1.0f));

				Font::Sptr font = ResourceManager::CreateAsset<Font>("fonts/Roboto-Medium.ttf", 36.0f);
				font->Bake();

				GuiText::Sptr text = subPanel3->Add<GuiText>();
				text->SetText("Exit Game");
				text->SetFont(font);

			}
			canvas5->AddChild(subPanel3);

			GameObject::Sptr subPanel4 = scene->CreateGameObject("Win Title");
			{
				RectTransform::Sptr transform = subPanel4->Add<RectTransform>();
				transform->SetMin({ 10, 10 });
				transform->SetMax({ 590, 128 });
				transform->SetPosition(glm::vec2(300.0f, 100.0f));

				GuiPanel::Sptr panel = subPanel4->Add<GuiPanel>();
				panel->SetColor(glm::vec4(1.0f, 1.0f, 0.0f, 1.0f));

				Font::Sptr font = ResourceManager::CreateAsset<Font>("fonts/Roboto-Medium.ttf", 36.0f);
				font->Bake();

				GuiText::Sptr text = subPanel4->Add<GuiText>();
				text->SetText("YOU WIN!");
				text->SetFont(font);

			}
			canvas5->AddChild(subPanel4);
		}

		GameObject::Sptr canvas6 = scene->CreateGameObject("Lose");
		{
			RectTransform::Sptr transform = canvas6->Add<RectTransform>();
			transform->SetMin({ 100, 100 });
			transform->SetMax({ 700, 800 });
			transform->SetPosition(glm::vec2(400.0f, 400.0f));

			GuiPanel::Sptr canPanel = canvas6->Add<GuiPanel>();
			canPanel->SetColor(glm::vec4(0.6f, 0.3f, 0.0f, 1.0f));

			GameObject::Sptr subPanel2 = scene->CreateGameObject("FinalScoreL");
			{
				RectTransform::Sptr transform = subPanel2->Add<RectTransform>();
				transform->SetMin({ 10, 10 });
				transform->SetMax({ 590, 128 });
				transform->SetPosition(glm::vec2(300.0f, 450.0f));

				GuiPanel::Sptr panel = subPanel2->Add<GuiPanel>();
				panel->SetColor(glm::vec4(0.3f, 0.15f, 0.0f, 1.0f));

				Font::Sptr font = ResourceManager::CreateAsset<Font>("fonts/Roboto-Medium.ttf", 36.0f);
				font->Bake();

				GuiText::Sptr text = subPanel2->Add<GuiText>();
				text->SetText("0");
				text->SetFont(font);

			}
			canvas6->AddChild(subPanel2);

			GameObject::Sptr subPanel3 = scene->CreateGameObject("Button10");
			{
				RectTransform::Sptr transform = subPanel3->Add<RectTransform>();
				transform->SetMin({ 10, 10 });
				transform->SetMax({ 590, 128 });
				transform->SetPosition(glm::vec2(300.0f, 600.0f));

				GuiPanel::Sptr panel = subPanel3->Add<GuiPanel>();
				panel->SetColor(glm::vec4(0.3f, 0.15f, 0.0f, 1.0f));

				Font::Sptr font = ResourceManager::CreateAsset<Font>("fonts/Roboto-Medium.ttf", 36.0f);
				font->Bake();

				GuiText::Sptr text = subPanel3->Add<GuiText>();
				text->SetText("Exit Game");
				text->SetFont(font);

			}
			canvas6->AddChild(subPanel3);

			GameObject::Sptr subPanel4 = scene->CreateGameObject("Win Title");
			{
				RectTransform::Sptr transform = subPanel4->Add<RectTransform>();
				transform->SetMin({ 10, 10 });
				transform->SetMax({ 590, 128 });
				transform->SetPosition(glm::vec2(300.0f, 100.0f));

				GuiPanel::Sptr panel = subPanel4->Add<GuiPanel>();
				panel->SetColor(glm::vec4(1.0f, 1.0f, 0.0f, 1.0f));

				Font::Sptr font = ResourceManager::CreateAsset<Font>("fonts/Roboto-Medium.ttf", 36.0f);
				font->Bake();

				GuiText::Sptr text = subPanel4->Add<GuiText>();
				text->SetText("GAME OVER!");
				text->SetFont(font);

			}
			canvas6->AddChild(subPanel4);
		}
		GuiBatcher::SetDefaultTexture(ResourceManager::CreateAsset<Texture2D>("textures/ui-sprite.png"));
		GuiBatcher::SetDefaultBorderRadius(8);


		// Call scene awake to start up all of our components
		scene->Window = window;
		scene->Awake();

		// Save the asset manifest for all the resources we just loaded
		ResourceManager::SaveManifest("manifest.json");
		// Save the scene to a JSON file
		scene->Save("scene.json");
	}
}

int main() {
	Logger::Init(); // We'll borrow the logger from the toolkit, but we need to initialize it

	//Initialize GLFW
	if (!initGLFW())
		return 1;

	//Initialize GLAD
	if (!initGLAD())
		return 1;

	// Let OpenGL know that we want debug output, and route it to our handler function
	glEnable(GL_DEBUG_OUTPUT);
	glEnable(GL_DEBUG_OUTPUT_SYNCHRONOUS);
	glDebugMessageCallback(GlDebugMessage, nullptr);

	// Initialize our ImGui helper
	ImGuiHelper::Init(window);

	// Initialize our resource manager
	ResourceManager::Init();

	// Register all our resource types so we can load them from manifest files
	ResourceManager::RegisterType<Texture2D>();
	ResourceManager::RegisterType<TextureCube>();
	ResourceManager::RegisterType<Shader>();
	ResourceManager::RegisterType<Material>();
	ResourceManager::RegisterType<MeshResource>();

	// Register all of our component types so we can load them from files
	ComponentManager::RegisterType<Camera>();
	ComponentManager::RegisterType<RenderComponent>();
	ComponentManager::RegisterType<RigidBody>();
	ComponentManager::RegisterType<TriggerVolume>();
	ComponentManager::RegisterType<RotatingBehaviour>();
	ComponentManager::RegisterType<JumpBehaviour>();
	ComponentManager::RegisterType<MaterialSwapBehaviour>();
	ComponentManager::RegisterType<TriggerVolumeEnterBehaviour>();
	ComponentManager::RegisterType<SimpleCameraControl>();

	ComponentManager::RegisterType<RectTransform>();
	ComponentManager::RegisterType<GuiPanel>();
	ComponentManager::RegisterType<GuiText>();

	// GL states, we'll enable depth testing and backface fulling
	glEnable(GL_DEPTH_TEST);
	glEnable(GL_CULL_FACE);
	glCullFace(GL_BACK);
	glClearColor(0.2f, 0.2f, 0.2f, 1.0f);


	// Structure for our frame-level uniforms, matches layout from
	// fragments/frame_uniforms.glsl
	// For use with a UBO.
	struct FrameLevelUniforms {
		// The camera's view matrix
		glm::mat4 u_View;
		// The camera's projection matrix
		glm::mat4 u_Projection;
		// The combined viewProject matrix
		glm::mat4 u_ViewProjection;
		// The camera's position in world space
		glm::vec4 u_CameraPos;
		// The time in seconds since the start of the application
		float u_Time;
	};
	// This uniform buffer will hold all our frame level uniforms, to be shared between shaders
	UniformBuffer<FrameLevelUniforms>::Sptr frameUniforms = std::make_shared<UniformBuffer<FrameLevelUniforms>>(BufferUsage::DynamicDraw);
	// The slot that we'll bind our frame level UBO to
	const int FRAME_UBO_BINDING = 0;

	// Structure for our isntance-level uniforms, matches layout from
	// fragments/frame_uniforms.glsl
	// For use with a UBO.
	struct InstanceLevelUniforms {
		// Complete MVP
		glm::mat4 u_ModelViewProjection;
		// Just the model transform, we'll do worldspace lighting
		glm::mat4 u_Model;
		// Normal Matrix for transforming normals
		glm::mat4 u_NormalMatrix;
	};

	// This uniform buffer will hold all our instance level uniforms, to be shared between shaders
	UniformBuffer<InstanceLevelUniforms>::Sptr instanceUniforms = std::make_shared<UniformBuffer<InstanceLevelUniforms>>(BufferUsage::DynamicDraw);

	// The slot that we'll bind our instance level UBO to
	const int INSTANCE_UBO_BINDING = 1;
	//randomized seed
	srand(time(NULL));


	//randomized skybox
	randomizedSkybox = rand() % 3 + 1;
	std::cout << randomizedSkybox << std::endl;

	////////////////////////////////
	///// SCENE CREATION MOVED /////
	////////////////////////////////
	CreateScene();

	// We'll use this to allow editing the save/load path
	// via ImGui, note the reserve to allocate extra space
	// for input!
	std::string scenePath = "scene.json"; 
	scenePath.reserve(256); 

	// Our high-precision timer
	double lastFrame = glfwGetTime();

	BulletDebugMode physicsDebugMode = BulletDebugMode::None;
	float playbackSpeed = 1.0f;

	nlohmann::json editorSceneState;

	int health = 100;
	int lane = 1, spawn = 1, menuSelect = 1, menuType = 1, score = 0;
	//menuType 1 = main menu
	//menuType 2 = settings
	//menuType 3 = pause menu/in game
	//menuType 4 = win screen
	//menuType 5 = lose screen
	float rotateTo = 0.0f, newRotate = 0.0f, goblinPos = 0.0f;
	bool isButtonPressed = false, isRotate = false, rotateDir = false, newSpawn = false, isGameRunning = false, startUp = true;

	//if shootTimer <= 0 can shoot, reset to shooTime
	bool canShoot = true, charging = false;
	float shootTimer = 0.f, shootTime = 2.f;
	float shootPower = 5.0f, powerLevel = 0.0f, powerOffset = 630.0f;

	spawn = rand() % 4 + 1;

	
	///// Game loop /////
	while (!glfwWindowShouldClose(window)) {
		glfwPollEvents();
		ImGuiHelper::StartFrame();

		Camera::Sptr camera = scene->MainCamera;
		GameObject::Sptr goblin = scene->FindObjectByName("goblin1");
		GameObject::Sptr cannonBall = scene->FindObjectByName("cannonBall");
		GameObject::Sptr mainMenu = scene->FindObjectByName("Main Menu");
		GameObject::Sptr mainMenuB1 = scene->FindObjectByName("Button1");
		GameObject::Sptr mainMenuB2 = scene->FindObjectByName("Button2");
		GameObject::Sptr mainMenuB3 = scene->FindObjectByName("Button3");

		GameObject::Sptr settingsMenu = scene->FindObjectByName("Settings Menu");
		GameObject::Sptr settingsMenuB1 = scene->FindObjectByName("Button4");
		GameObject::Sptr settingsMenuB2 = scene->FindObjectByName("Button5");
		GameObject::Sptr settingsMenuB3 = scene->FindObjectByName("Button6");

		GameObject::Sptr inGame = scene->FindObjectByName("inGameGUI");
		GameObject::Sptr inGameScore = scene->FindObjectByName("Score");
		GameObject::Sptr inGamePower = scene->FindObjectByName("Charge Level");
		GameObject::Sptr inGameHealth = scene->FindObjectByName("Health Level");

		GameObject::Sptr pauseMenu = scene->FindObjectByName("Pause Menu");
		GameObject::Sptr pauseMenuB1 = scene->FindObjectByName("Button7");
		GameObject::Sptr pauseMenuB2 = scene->FindObjectByName("Button8");

		GameObject::Sptr winMenu = scene->FindObjectByName("Win");
		GameObject::Sptr winMenuScore = scene->FindObjectByName("FinalScoreW");
		GameObject::Sptr winMenuB1 = scene->FindObjectByName("Button9");

		GameObject::Sptr loseMenu = scene->FindObjectByName("Lose");
		GameObject::Sptr loseMenuScore = scene->FindObjectByName("FinalScoreL");
		GameObject::Sptr loseMenuB1 = scene->FindObjectByName("Button10");


		// Calculate the time since our last frame (dt)
		double thisFrame = glfwGetTime();
		float dt = static_cast<float>(thisFrame - lastFrame);

		//GUI Startup
		if (startUp == true)
		{
			settingsMenu->SetEnabled(false);
			inGame->SetEnabled(false);
			pauseMenu->SetEnabled(false);
			winMenu->SetEnabled(false);
			loseMenu->SetEnabled(false);
			startUp = false;
		}

		//menu systems
		if (isGameRunning == false)
		{
			//main menu selection color
			if (menuType == 1)
			{
				//main menu button selection 
				if (menuSelect == 1)
				{
					mainMenuB1->Get<GuiPanel>()->SetColor(glm::vec4(0.0f, 1.0f, 0.0f, 1.0f));
				}
				else
				{
					mainMenuB1->Get<GuiPanel>()->SetColor(glm::vec4(0.3f, 0.15f, 0.0f, 1.0f));
					//mainMenuB1->Get<GuiPanel>()->SetColor(glm::vec4(1.0f));
				}
				if (menuSelect == 2)
				{
					mainMenuB2->Get<GuiPanel>()->SetColor(glm::vec4(0.0f, 1.0f, 0.0f, 1.0f));
				}
				else
				{
					mainMenuB2->Get<GuiPanel>()->SetColor(glm::vec4(0.3f, 0.15f, 0.0f, 1.0f));
				}
				if (menuSelect == 3)
				{
					mainMenuB3->Get<GuiPanel>()->SetColor(glm::vec4(0.0f, 1.0f, 0.0f, 1.0f));
				}
				else
				{
					mainMenuB3->Get<GuiPanel>()->SetColor(glm::vec4(0.3f, 0.15f, 0.0f, 1.0f));
				}
			}
			//settings selection color
			else if (menuType == 2)
			{
				//main menu button selection 
				if (menuSelect == 1)
				{
					settingsMenuB1->Get<GuiPanel>()->SetColor(glm::vec4(0.0f, 1.0f, 0.0f, 1.0f));
				}
				else
				{
					settingsMenuB1->Get<GuiPanel>()->SetColor(glm::vec4(0.3f, 0.15f, 0.0f, 1.0f));
				}
				if (menuSelect == 2)
				{
					settingsMenuB2->Get<GuiPanel>()->SetColor(glm::vec4(0.0f, 1.0f, 0.0f, 1.0f));
				}
				else
				{
					settingsMenuB2->Get<GuiPanel>()->SetColor(glm::vec4(0.3f, 0.15f, 0.0f, 1.0f));
				}
				if (menuSelect == 3)
				{
					settingsMenuB3->Get<GuiPanel>()->SetColor(glm::vec4(0.0f, 1.0f, 0.0f, 1.0f));
				}
				else
				{
					settingsMenuB3->Get<GuiPanel>()->SetColor(glm::vec4(0.3f, 0.15f, 0.0f, 1.0f));
				}
			}
			//pause selection color
			else if (menuType == 4)
			{
				if (menuSelect == 1)
				{
					pauseMenuB1->Get<GuiPanel>()->SetColor(glm::vec4(0.0f, 1.0f, 0.0f, 1.0f));
				}
				else
				{
					pauseMenuB1->Get<GuiPanel>()->SetColor(glm::vec4(0.3f, 0.15f, 0.0f, 1.0f));
				}
				if (menuSelect == 2)
				{
					pauseMenuB2->Get<GuiPanel>()->SetColor(glm::vec4(0.0f, 1.0f, 0.0f, 1.0f));
				}
				else
				{
					pauseMenuB2->Get<GuiPanel>()->SetColor(glm::vec4(0.3f, 0.15f, 0.0f, 1.0f));
				}
			}
			//win screen selection color
			else if (menuType == 5)
			{
				winMenuB1->Get<GuiPanel>()->SetColor(glm::vec4(0.0f, 1.0f, 0.0f, 1.0f));
			}
			//lose screen selection color
			else if (menuType == 6)
			{
				loseMenuB1->Get<GuiPanel>()->SetColor(glm::vec4(0.0f, 1.0f, 0.0f, 1.0f));
			}
			

			if (glfwGetKey(window, GLFW_KEY_UP)) //select up
			{
				if (!isButtonPressed)
				{
					if (menuType == 4)
					{
						if (menuSelect == 1)
						{
							menuSelect = 2;
						}
						else
						{
							menuSelect = 1;
						}
					}
					else
					{
						if (menuSelect == 1)
						{
							menuSelect = 3;
						}
						else
						{
							menuSelect--;
						}
					}
				}
				isButtonPressed = true;
			}
			else if (glfwGetKey(window, GLFW_KEY_DOWN)) //select down
			{
				if (!isButtonPressed)
				{
					if (menuType == 4)
					{
						if (menuSelect == 2)
						{
							menuSelect = 1;
						}
						else
						{
							menuSelect = 2;
						}
					}
					else
					{
						if (menuSelect == 3)
						{
							menuSelect = 1;
						}
						else
						{
							menuSelect++;
						}
					}
				}
				isButtonPressed = true;
			}
			else if (glfwGetKey(window, GLFW_KEY_ENTER))  //button selection
			{
				if (!isButtonPressed)
				{
					//main menu
					if (menuType == 1)
					{
						if (menuSelect == 1)
						{
							mainMenu->SetEnabled(false);
							inGame->SetEnabled(true);
							inGame->RenderGUI();
							menuType = 3;
							isGameRunning = true;
							scene->IsPlaying = true;
						}
						if (menuSelect == 2)
						{
							mainMenu->SetEnabled(false);
							settingsMenu->SetEnabled(true);
							settingsMenu->RenderGUI();
							menuType = 2;
						}
						if (menuSelect == 3)
						{
							return(0);
						}
					}
					//settings
					else if (menuType == 2)
					{
						if (menuSelect == 3)
						{
							settingsMenu->SetEnabled(false);
							mainMenu->SetEnabled(true);
							mainMenu->RenderGUI();
							menuType = 1;
						}
					}
					//pause menu
					else if (menuType == 4)
					{
						if (menuSelect == 1)
						{
							return(0);
						}
						else if (menuSelect == 2)
						{
							pauseMenu->SetEnabled(false);
							inGame->SetEnabled(true);
							inGame->RenderGUI();
							menuType = 3;
							isGameRunning = true;
						}
					}
					//win screen
					else if (menuType == 5)
					{
						return(0);
					}
					//lose screen
					else if (menuType == 6)
					{
						return(0);
					}
				}
				isButtonPressed = true;
			}
			else
			{
				isButtonPressed = false;
			}
		}
		//game systems
		else if (isGameRunning == true)
		{
			if (shootTimer <= 0) canShoot = true;
			else shootTimer -= dt;
			if (menuType == 3)
			{
				if (glfwGetKey(window, GLFW_KEY_N)) //this is for the score counter change it to add score when enemy has been defeated
				{
					if (!isButtonPressed)
					{
						score += 10;
						inGameScore->Get<GuiText>()->SetText(std::to_string(score));
					}
					isButtonPressed = true;
				}
				else if (glfwGetKey(window, GLFW_KEY_K)) //this is for the win condition change it to trigger when needed
				{
					if (!isButtonPressed)
					{
						inGame->SetEnabled(false);
						winMenu->SetEnabled(true);
						winMenuScore->Get<GuiText>()->SetText("Final Score: " + std::to_string(score));
						isGameRunning = false;
						menuType = 5;
					}
					isButtonPressed = true;
				}
				else if (glfwGetKey(window, GLFW_KEY_P))  //pause button
				{
					if (!isButtonPressed)
					{
						pauseMenu->SetEnabled(true);
						inGame->SetEnabled(false);
						isGameRunning = false;
						menuType = 4;
					}
					isButtonPressed = true;
				}
				else
				{
					isButtonPressed = false;
				}
			}

			if (glfwGetKey(window, GLFW_KEY_A))
			{
				if (!isButtonPressed && !isRotate)
				{
					isRotate = true;
					rotateDir = true;
					if (lane == 4)
					{
						lane = 1;
					}
					else
					{
						lane++;
					}
					switch (lane)
					{
					case 1:
						rotateTo = 360.0f;
						newRotate = 270.0f;
						break;
					case 2:
						rotateTo = 90.0f;
						newRotate = 0.0f;
						break;
					case 3:
						rotateTo = 180.0f;
						newRotate = 90.0f;
						break;
					case 4:
						rotateTo = 270.0f;
						newRotate = 180.0f;
						break;
					default:
						break;
					}
				}
				isButtonPressed = true;
			}
			else if (glfwGetKey(window, GLFW_KEY_D))
			{
				if (!isButtonPressed && !isRotate)
				{
					isRotate = true;
					rotateDir = false;
					if (lane == 1)
					{
						lane = 4;
					}
					else
					{
						lane--;
					}
					switch (lane)
					{
					case 1:
						rotateTo = 0.0f;
						newRotate = 90.0f;
						break;
					case 2:
						rotateTo = 90.0f;
						newRotate = 180.0f;
						break;
					case 3:
						rotateTo = 180.0f;
						newRotate = 270.0f;
						break;
					case 4:
						rotateTo = 270.0f;
						newRotate = 360.0f;
						break;
					default:
						break;
					}
				}
				isButtonPressed = true;
			}
			else
			{
				isButtonPressed = false;
			}

			///Camera Rotation///
			if (isRotate)
			{
				if (rotateDir)
				{
					newRotate += dt * 100;
					camera->GetGameObject()->SetRotation(glm::vec3(90.0f, 0.0f, newRotate));
					if (newRotate >= rotateTo)
					{
						isRotate = false;
					}
					if (newRotate >= 360.0f)
					{
						camera->GetGameObject()->SetRotation(glm::vec3(90.0f, 0.0f, 0.0f));
					}
				}
				else if (!rotateDir)
				{
					newRotate -= dt * 100;
					camera->GetGameObject()->SetRotation(glm::vec3(90.0f, 0.0f, newRotate));
					if (newRotate <= rotateTo)
					{
						isRotate = false;
					}
					if (newRotate <= 0.0f)
					{
						camera->GetGameObject()->SetRotation(glm::vec3(90.0f, 0.0f, 360.0f));
					}
				}
			}

			//////Shooting///////
			if (glfwGetKey(window, GLFW_KEY_SPACE) && canShoot) {
				//shoot then reset wait timer
				if (shootPower < 70)
				{
					shootPower += dt * 20.0f;
					powerOffset += dt * 21.5f;
				}
				else
				{
					shootPower = 70.0f;
				}
				charging = true;
				powerLevel = (shootPower / 70);
				inGamePower->Get<RectTransform>()->SetMin({ 0, 10 });
				inGamePower->Get<RectTransform>()->SetMax({ 150 * powerLevel, 20 });
				
				inGamePower->Get<RectTransform>()->SetPosition(glm::vec2(powerOffset, 780.0f));
				//150
			}
			else
			{
				if (charging == true)
				{
					switch (lane)
					{
					case 1:
						cannonBall->SetPostion(glm::vec3(12.760f, -9.f, 5.f));
						cannonBall->Get<RigidBody>()->Awake();
						cannonBall->Get<RigidBody>()->ApplyImpulse(glm::vec3(0.0f, shootPower, 25.0f));
						break;
					case 2:
						cannonBall->SetPostion(glm::vec3(11.f, -10.5f, 5.f));
						cannonBall->Get<RigidBody>()->Awake();
						cannonBall->Get<RigidBody>()->ApplyImpulse(glm::vec3((-1 * shootPower), 0.0f, 25.0f));
						break;
					case 3:
						cannonBall->SetPostion(glm::vec3(12.760f, -12.f, 5.f));
						cannonBall->Get<RigidBody>()->Awake();
						cannonBall->Get<RigidBody>()->ApplyImpulse(glm::vec3(0.0f, (-1*shootPower), 25.0f));
						break;
					case 4:
						cannonBall->SetPostion(glm::vec3(14.f, -10.5f, 5.f));
						cannonBall->Get<RigidBody>()->Awake();
						cannonBall->Get<RigidBody>()->ApplyImpulse(glm::vec3(shootPower, 0.0f, 25.0f));
						break;
					default:
						break;
					}
					canShoot = false;
					shootTimer = shootTime;
					shootPower = 5.0f;
					charging = false;
					powerOffset = 630.0f;
					inGamePower->Get<RectTransform>()->SetMin({ 0, 10 });
					inGamePower->Get<RectTransform>()->SetMax({ 10, 20 });
					inGamePower->Get<RectTransform>()->SetPosition(glm::vec2(630.0f, 780.0f));
				}
			}

			//////Enemy Spawning//////
			switch (spawn)
			{
			case 1:		//lane 1	
				if (newSpawn == false)
				{
					goblin->SetRotation(glm::vec3(90.0f, 0.0f, -90.0f));
					goblin->SetPostion(glm::vec3(12.760f, 11.0f, 1.0f));
					goblinPos = 11.0f;
					newSpawn = true;
				}

				if (goblin->GetPosY() <= -10.420f)
				{
					spawn = rand() % 4 + 1;
					newSpawn = false;
					break;
				}
				else
				{
					goblinPos = goblin->GetPosY();
					goblinPos -= dt * 2;
					goblin->SetPostion(glm::vec3(12.760f, goblinPos, 1.0f));
					break;
				}
			case 2:		//lane 2

				if (newSpawn == false)
				{
					goblin->SetRotation(glm::vec3(90.0f, 0.0f, 0.0f));
					goblin->SetPostion(glm::vec3(-9.0f, -10.420f, 1.0f));
					goblinPos = -9.0f;
					newSpawn = true;
				}

				if (goblin->GetPosX() >= 12.760f)
				{
					spawn = rand() % 4 + 1;
					newSpawn = false;
					break;
				}
				else
				{
					goblinPos = goblin->GetPosX();
					goblinPos += dt * 2;
					goblin->SetPostion(glm::vec3(goblinPos, -10.420f, 1.0f));
					break;
				}
			case 3:		//lane 3	
				if (newSpawn == false)
				{
					goblin->SetRotation(glm::vec3(90.0f, 0.0f, 90.0f));
					goblin->SetPostion(glm::vec3(12.760f, -32.0f, 1.0f));
					goblinPos = -32.0f;
					newSpawn = true;
				}

				if (goblin->GetPosY() >= -10.420f)
				{
					spawn = rand() % 4 + 1;
					newSpawn = false;
					break;
				}
				else
				{
					goblinPos = goblin->GetPosY();
					goblinPos += dt * 2;
					goblin->SetPostion(glm::vec3(12.760f, goblinPos, 1.0f));
					break;
				}
			case 4:		//lane 4	
				if (newSpawn == false)
				{
					goblin->SetRotation(glm::vec3(90.0f, 0.0f, 180.0f));
					goblin->SetPostion(glm::vec3(35.0f, -10.420f, 1.0f));
					goblinPos = 35.0f;
					newSpawn = true;
				}

				if (goblin->GetPosX() <= 12.760f)
				{
					spawn = rand() % 4 + 1;
					newSpawn = false;
					break;
				}
				else
				{
					goblinPos = goblin->GetPosX();
					goblinPos -= dt * 2;
					goblin->SetPostion(glm::vec3(goblinPos, -10.420f, 1.0f));
					break;
				}
			default:
				break;
			}
			//health decrements for now
			if (goblin->GetPosX() <= 13.f && goblin->GetPosX() >= 11.f && goblin->GetPosY() <= -9.f && goblin->GetPosY() >= -11.f) {
				health -= 10;
				spawn = rand() % 4 + 1;
				newSpawn = false;
				std::cout << health << std::endl;

				float healthLevel = (health / 100);
				inGameHealth->Get<RectTransform>()->SetMin({ 0, 10 });
				inGameHealth->Get<RectTransform>()->SetMax({ 100.f * healthLevel, 20 });
				inGameHealth->Get<RectTransform>()->SetPosition((glm::vec2(100.0f, 35.0f)));

			}
			if (health <= 0) {
				std::cout << "Game end, you is ded\n";
				inGame->SetEnabled(false);
				winMenu->SetEnabled(true);
				winMenuScore->Get<GuiText>()->SetText("Final Score: " + std::to_string(score));
				isGameRunning = false;
				menuType = 5;
			}
		}
		// Draw our material properties window!
		DrawMaterialsWindow();

		// Showcasing how to use the imGui library!
		bool isDebugWindowOpen = ImGui::Begin("Debugging");
		if (isDebugWindowOpen) {
			// Draws a button to control whether or not the game is currently playing
			static char buttonLabel[64];
			sprintf_s(buttonLabel, "%s###playmode", scene->IsPlaying ? "Exit Play Mode" : "Enter Play Mode");
			if (ImGui::Button(buttonLabel)) {
				// Save scene so it can be restored when exiting play mode
				if (!scene->IsPlaying) {
					editorSceneState = scene->ToJson();
				}

				// Toggle state
				scene->IsPlaying = !scene->IsPlaying;

				// If we've gone from playing to not playing, restore the state from before we started playing
				if (!scene->IsPlaying) {
					scene = nullptr;
					// We reload to scene from our cached state
					scene = Scene::FromJson(editorSceneState);
					// Don't forget to reset the scene's window and wake all the objects!
					scene->Window = window;
					scene->Awake();
				}
			}

			// Make a new area for the scene saving/loading
			ImGui::Separator();
			if (DrawSaveLoadImGui(scene, scenePath)) {
				// C++ strings keep internal lengths which can get annoying
				// when we edit it's underlying datastore, so recalcualte size
				scenePath.resize(strlen(scenePath.c_str()));

				// We have loaded a new scene, call awake to set
				// up all our components
				scene->Window = window;
				scene->Awake();
			}
			ImGui::Separator();
			// Draw a dropdown to select our physics debug draw mode
			if (BulletDebugDraw::DrawModeGui("Physics Debug Mode:", physicsDebugMode)) {
				scene->SetPhysicsDebugDrawMode(physicsDebugMode);
			}
			LABEL_LEFT(ImGui::SliderFloat, "Playback Speed:    ", &playbackSpeed, 0.0f, 10.0f);
			ImGui::Separator();
		}

		// Clear the color and depth buffers
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

		ImGui::Text("Lane: %d", lane);
		ImGui::Separator();

		// Draw some ImGui stuff for the lights
		if (isDebugWindowOpen) {
			for (int ix = 0; ix < scene->Lights.size(); ix++) {
				char buff[256];
				sprintf_s(buff, "Light %d##%d", ix, ix);
				// DrawLightImGui will return true if the light was deleted
				if (DrawLightImGui(scene, buff, ix)) {
					// Remove light from scene, restore all lighting data
					scene->Lights.erase(scene->Lights.begin() + ix);
					scene->SetupShaderAndLights();
					// Move back one element so we don't skip anything!
					ix--;
				}
			}
			// As long as we don't have max lights, draw a button
			// to add another one
			if (scene->Lights.size() < scene->MAX_LIGHTS) {
				if (ImGui::Button("Add Light")) {
					scene->Lights.push_back(Light());
					scene->SetupShaderAndLights();
				}
			}
			// Split lights from the objects in ImGui
			ImGui::Separator();
		}

		dt *= playbackSpeed;

		// Perform updates for all components
		scene->Update(dt);

		// Grab shorthands to the camera and shader from the scene
		//Camera::Sptr camera = scene->MainCamera;

		// Cache the camera's viewprojection
		glm::mat4 viewProj = camera->GetViewProjection();
		DebugDrawer::Get().SetViewProjection(viewProj);

		// Update our worlds physics!
		scene->DoPhysics(dt);

		// Draw object GUIs
		if (isDebugWindowOpen) {
			scene->DrawAllGameObjectGUIs();
		}
		
		// The current material that is bound for rendering
		Material::Sptr currentMat = nullptr;
		Shader::Sptr shader = nullptr;

		// Bind the skybox texture to a reserved texture slot
		// See Material.h and Material.cpp for how we're reserving texture slots
		TextureCube::Sptr environment = scene->GetSkyboxTexture();
		if (environment) environment->Bind(0); 

		// Make sure depth testing and culling are re-enabled
		glEnable(GL_DEPTH_TEST);
		glEnable(GL_CULL_FACE);

		// Here we'll bind all the UBOs to their corresponding slots
		scene->PreRender();
		frameUniforms->Bind(FRAME_UBO_BINDING);
		instanceUniforms->Bind(INSTANCE_UBO_BINDING);

		// Upload frame level uniforms
		auto& frameData = frameUniforms->GetData();
		frameData.u_Projection = camera->GetProjection();
		frameData.u_View = camera->GetView();
		frameData.u_ViewProjection = camera->GetViewProjection();
		frameData.u_CameraPos = glm::vec4(camera->GetGameObject()->GetPosition(), 1.0f);
		frameData.u_Time = static_cast<float>(thisFrame);
		frameUniforms->Update();

		// Render all our objects
		ComponentManager::Each<RenderComponent>([&](const RenderComponent::Sptr& renderable) {
			// Early bail if mesh not set
			if (renderable->GetMesh() == nullptr) { 
				return;
			}

			// If we don't have a material, try getting the scene's fallback material
			// If none exists, do not draw anything
			if (renderable->GetMaterial() == nullptr) {
				if (scene->DefaultMaterial != nullptr) {
					renderable->SetMaterial(scene->DefaultMaterial);
				} else {
					return;
				}
			}

			// If the material has changed, we need to bind the new shader and set up our material and frame data
			// Note: This is a good reason why we should be sorting the render components in ComponentManager
			if (renderable->GetMaterial() != currentMat) {
				currentMat = renderable->GetMaterial();
				shader = currentMat->GetShader();

				shader->Bind();
				currentMat->Apply();
			}

			// Grab the game object so we can do some stuff with it
			GameObject* object = renderable->GetGameObject();
			 
			// Use our uniform buffer for our instance level uniforms
			auto& instanceData = instanceUniforms->GetData();
			instanceData.u_Model = object->GetTransform();
			instanceData.u_ModelViewProjection = viewProj * object->GetTransform();
			instanceData.u_NormalMatrix = glm::mat3(glm::transpose(glm::inverse(object->GetTransform())));
			instanceUniforms->Update();  

			// Draw the object
			renderable->GetMesh()->Draw();
		});

		// Use our cubemap to draw our skybox
		scene->DrawSkybox();

		// Disable culling
		glDisable(GL_CULL_FACE);
		// Disable depth testing, we're going to use order-dependant layering
		glDisable(GL_DEPTH_TEST);
		// Disable depth writing
		glDepthMask(GL_FALSE);

		// Enable alpha blending
		glEnable(GL_BLEND);
		glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

		// Enable the scissor test;
		glEnable(GL_SCISSOR_TEST);

		// Our projection matrix will be our entire window for now
		glm::mat4 proj = glm::ortho(0.0f, (float)windowSize.x, (float)windowSize.y, 0.0f, -1.0f, 1.0f);
		GuiBatcher::SetProjection(proj);

		// Iterate over and render all the GUI objects
		scene->RenderGUI();

		// Flush the Gui Batch renderer
		GuiBatcher::Flush();

		// Disable alpha blending
		glDisable(GL_BLEND);
		// Disable scissor testing
		glDisable(GL_SCISSOR_TEST);
		// Re-enable depth writing
		glDepthMask(GL_TRUE);

		// End our ImGui window
		ImGui::End();

		VertexArrayObject::Unbind();

		lastFrame = thisFrame;
		ImGuiHelper::EndFrame();
		InputEngine::EndFrame();
		glfwSwapBuffers(window);
	}

	// Clean up the ImGui library
	ImGuiHelper::Cleanup();

	// Clean up the resource manager
	ResourceManager::Cleanup();

	// Clean up the toolkit logger so we don't leak memory
	Logger::Uninitialize();
	return 0;
}