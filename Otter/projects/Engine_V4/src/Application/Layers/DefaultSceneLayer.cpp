#include "DefaultSceneLayer.h"

// GLM math library
#include <GLM/glm.hpp>
#include <GLM/gtc/matrix_transform.hpp>
#include <GLM/gtc/type_ptr.hpp>
#include <GLM/gtc/random.hpp>
#define GLM_ENABLE_EXPERIMENTAL
#include <GLM/gtx/common.hpp> // for fmod (floating modulus)

#include <filesystem>
#include "Application/Timing.h"

// Graphics
#include "Graphics/Buffers/IndexBuffer.h"
#include "Graphics/Buffers/VertexBuffer.h"
#include "Graphics/VertexArrayObject.h"
#include "Graphics/ShaderProgram.h"
#include "Graphics/Textures/Texture2D.h"
#include "Graphics/Textures/TextureCube.h"
#include "Graphics/VertexTypes.h"
#include "Graphics/Font.h"
#include "Graphics/GuiBatcher.h"
#include "Graphics/Framebuffer.h"



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
#include "ToneFire.h"

// Gameplay
#include "Gameplay/Material.h"
#include "Gameplay/GameObject.h"
#include "Gameplay/Scene.h"
#include "Gameplay/Components/Light.h"

// Components
#include "Gameplay/Components/IComponent.h"
#include "Gameplay/Components/Camera.h"
#include "Gameplay/Components/RotatingBehaviour.h"
#include "Gameplay/Components/JumpBehaviour.h"
#include "Gameplay/Components/RenderComponent.h"
#include "Gameplay/Components/MaterialSwapBehaviour.h"
#include "Gameplay/Components/TriggerVolumeEnterBehaviour.h"
#include "Gameplay/Components/SimpleCameraControl.h"
#include "Gameplay/Components/EnemyMovement.h"
#include "Gameplay/Components/CameraVanguard.h"

// Physics
#include "Gameplay/Physics/RigidBody.h"
#include "Gameplay/Physics/Colliders/BoxCollider.h"
#include "Gameplay/Physics/Colliders/PlaneCollider.h"
#include "Gameplay/Physics/Colliders/SphereCollider.h"
#include "Gameplay/Physics/Colliders/ConvexMeshCollider.h"
#include "Gameplay/Physics/Colliders/CylinderCollider.h"
#include "Gameplay/Physics/TriggerVolume.h"
#include "Graphics/DebugDraw.h"

// GUI
#include "Gameplay/Components/GUI/RectTransform.h"
#include "Gameplay/Components/GUI/GuiPanel.h"
#include "Gameplay/Components/GUI/GuiText.h"
#include "Gameplay/InputEngine.h"

#include "Application/Application.h"
#include "Gameplay/Components/ParticleSystem.h"
#include "Graphics/Textures/Texture3D.h"
#include "Graphics/Textures/Texture1D.h"

DefaultSceneLayer::DefaultSceneLayer() :
	ApplicationLayer()
{
	Name = "Default Scene";
	Overrides = AppLayerFunctions::OnAppLoad | AppLayerFunctions::OnUpdate;
}

DefaultSceneLayer::~DefaultSceneLayer() = default;

void DefaultSceneLayer::OnAppLoad(const nlohmann::json& config) {
	_CreateScene();
}


double preFrame = glfwGetTime();
void DefaultSceneLayer::OnUpdate()
{

	Application& app = Application::Get();
	currScene = app.CurrentScene();

	double currFrame = glfwGetTime();
	float dt = static_cast<float>(currFrame - preFrame);

	if (!start)
	{
		if (InputEngine::GetKeyState(GLFW_KEY_ENTER) == ButtonState::Pressed)
		{
			sPressed = true;
			currScene->IsPlaying = true;

		}

		
		if (sPressed)
		{
			start = true;
			sPressed = false;
			//AudioEngine::playEvents("event:/Daytime Song");
		}
	}


	
}




void DefaultSceneLayer::_CreateScene()
{
	using namespace Gameplay;
	using namespace Gameplay::Physics;

	Application& app = Application::Get();

	bool loadScene = false;
	// For now we can use a toggle to generate our scene vs load from file
	if (loadScene && std::filesystem::exists("scene.json")) {
		app.LoadScene("scene.json");
	} else {

#pragma region Loading Shader Programs 
		// Basic gbuffer generation with no vertex manipulation
		ShaderProgram::Sptr deferredForward = ResourceManager::CreateAsset<ShaderProgram>(std::unordered_map<ShaderPartType, std::string>{
			{ ShaderPartType::Vertex, "shaders/vertex_shaders/basic.glsl" },
			{ ShaderPartType::Fragment, "shaders/fragment_shaders/deferred_forward.glsl" }
		});
		deferredForward->SetDebugName("Deferred - GBuffer Generation");  

		// Our foliage shader which manipulates the vertices of the mesh
		ShaderProgram::Sptr foliageShader = ResourceManager::CreateAsset<ShaderProgram>(std::unordered_map<ShaderPartType, std::string>{
			{ ShaderPartType::Vertex, "shaders/vertex_shaders/foliage.glsl" },
			{ ShaderPartType::Fragment, "shaders/fragment_shaders/deferred_forward.glsl" }
		});  
		foliageShader->SetDebugName("Foliage");   

		// This shader handles our multitexturing example
		ShaderProgram::Sptr multiTextureShader = ResourceManager::CreateAsset<ShaderProgram>(std::unordered_map<ShaderPartType, std::string>{
			{ ShaderPartType::Vertex, "shaders/vertex_shaders/vert_multitextured.glsl" },  
			{ ShaderPartType::Fragment, "shaders/fragment_shaders/frag_multitextured.glsl" }
		});
		multiTextureShader->SetDebugName("Multitexturing"); 

		// This shader handles our displacement mapping example
		ShaderProgram::Sptr displacementShader = ResourceManager::CreateAsset<ShaderProgram>(std::unordered_map<ShaderPartType, std::string>{
			{ ShaderPartType::Vertex, "shaders/vertex_shaders/displacement_mapping.glsl" },
			{ ShaderPartType::Fragment, "shaders/fragment_shaders/deferred_forward.glsl" }
		});
		displacementShader->SetDebugName("Displacement Mapping");

		// This shader handles our cel shading example
		ShaderProgram::Sptr celShader = ResourceManager::CreateAsset<ShaderProgram>(std::unordered_map<ShaderPartType, std::string>{
			{ ShaderPartType::Vertex, "shaders/vertex_shaders/displacement_mapping.glsl" },
			{ ShaderPartType::Fragment, "shaders/fragment_shaders/cel_shader.glsl" }
		});
		celShader->SetDebugName("Cel Shader");

#pragma endregion

#pragma region Loading Assets
		// Load in the meshes
		MeshResource::Sptr monkeyMesh = ResourceManager::CreateAsset<MeshResource>("models/Monkey.obj");
		MeshResource::Sptr shipMesh   = ResourceManager::CreateAsset<MeshResource>("models/fenrir.obj");

		//Our previous 3d assets
		MeshResource::Sptr towerGardenMesh = ResourceManager::CreateAsset<MeshResource>("models/FinalArea.obj");
		MeshResource::Sptr towerCannonMesh = ResourceManager::CreateAsset<MeshResource>("models/TowerV1.obj");
		MeshResource::Sptr cannonBallMesh = ResourceManager::CreateAsset<MeshResource>("models/Cannonball.obj");
		MeshResource::Sptr goblinMesh = ResourceManager::CreateAsset<MeshResource>("models/goblinfullrig.obj");
		MeshResource::Sptr spearMesh = ResourceManager::CreateAsset<MeshResource>("models/CubeTester.fbx");

		//Our new static 3D Assets
		MeshResource::Sptr winterGardenMesh = ResourceManager::CreateAsset<MeshResource>("models/WinterMap.obj");
		MeshResource::Sptr newGoblinMesh = ResourceManager::CreateAsset<MeshResource>("models/goblinsprint.obj");

		//Frame 1 of anims
		MeshResource::Sptr birdFlyMesh = ResourceManager::CreateAsset<MeshResource>("models/Animated/Bird/Birdfly_000001.obj");
		MeshResource::Sptr goblinAttackMesh = ResourceManager::CreateAsset<MeshResource>("models/Animated/Goblin/attack/GoblinAttack_000001.obj");
		MeshResource::Sptr oozeMesh = ResourceManager::CreateAsset<MeshResource>("models/Animated/Ooze/walk/oozewalk_000001.obj");
		MeshResource::Sptr zombieAttackMesh = ResourceManager::CreateAsset<MeshResource>("models/Animated/Zombie/attack/ZombieAttack_000001.obj");



		//Cannon
		MeshResource::Sptr cannonBarrelMesh = ResourceManager::CreateAsset<MeshResource>("models/Animated/Cannon/CannonBarrel.obj");
		MeshResource::Sptr cannonBaseMesh = ResourceManager::CreateAsset<MeshResource>("models/Animated/Cannon/CannonBase.obj");




		// Load in some textures
		Texture2D::Sptr    boxTexture   = ResourceManager::CreateAsset<Texture2D>("textures/box-diffuse.png");
		Texture2D::Sptr    boxSpec      = ResourceManager::CreateAsset<Texture2D>("textures/box-specular.png");
		Texture2D::Sptr    monkeyTex    = ResourceManager::CreateAsset<Texture2D>("textures/monkey-uvMap.png");
		Texture2D::Sptr    leafTex      = ResourceManager::CreateAsset<Texture2D>("textures/leaves.png");
		leafTex->SetMinFilter(MinFilter::Nearest);
		leafTex->SetMagFilter(MagFilter::Nearest); //ggggggggggggggggggg

		//our previous texture assets
		Texture2D::Sptr    gardenTowerTexture = ResourceManager::CreateAsset<Texture2D>("textures/YYY5.png");
		Texture2D::Sptr    redTex = ResourceManager::CreateAsset<Texture2D>("textures/red.png");
		Texture2D::Sptr    goblinTex = ResourceManager::CreateAsset<Texture2D>("textures/GoblinUVFill.png");

		//Our new texture assets
		Texture2D::Sptr    winterGardenTexture = ResourceManager::CreateAsset<Texture2D>("textures/WinterGardenTexture.png");

		//frame 1 animated textures
		Texture2D::Sptr    birdTexture = ResourceManager::CreateAsset<Texture2D>("textures/Animated/BirdUV.png");
		Texture2D::Sptr    goblinAttackTexture = ResourceManager::CreateAsset<Texture2D>("textures/Animated/GoblinUvComp.png");
		Texture2D::Sptr    oozeWalkTexture = ResourceManager::CreateAsset<Texture2D>("textures/Animated/oozeuvspot.png");
		Texture2D::Sptr    zombieTexture = ResourceManager::CreateAsset<Texture2D>("textures/Animated/ZombieUVblood.png");


		//cannon
		Texture2D::Sptr	   cannonBaseTexture = ResourceManager::CreateAsset<Texture2D>("textures/Animated/CannonWood.png");
		Texture2D::Sptr    cannonBarrelTexture = ResourceManager::CreateAsset<Texture2D>("textures/Animated/Cannon.png");
#pragma endregion

#pragma region Basic Texture Creation
		Texture2DDescription singlePixelDescriptor;
		singlePixelDescriptor.Width = singlePixelDescriptor.Height = 1;
		singlePixelDescriptor.Format = InternalFormat::RGB8;

		float normalMapDefaultData[3] = { 0.5f, 0.5f, 1.0f };
		Texture2D::Sptr normalMapDefault = ResourceManager::CreateAsset<Texture2D>(singlePixelDescriptor);
		normalMapDefault->LoadData(1, 1, PixelFormat::RGB, PixelType::Float, normalMapDefaultData);

		float solidBlack[3] = { 0.5f, 0.5f, 0.5f };
		Texture2D::Sptr solidBlackTex = ResourceManager::CreateAsset<Texture2D>(singlePixelDescriptor);
		solidBlackTex->LoadData(1, 1, PixelFormat::RGB, PixelType::Float, solidBlack);

		float solidGrey[3] = { 0.0f, 0.0f, 0.0f };
		Texture2D::Sptr solidGreyTex = ResourceManager::CreateAsset<Texture2D>(singlePixelDescriptor);
		solidGreyTex->LoadData(1, 1, PixelFormat::RGB, PixelType::Float, solidGrey);

		float solidWhite[3] = { 1.0f, 1.0f, 1.0f };
		Texture2D::Sptr solidWhiteTex = ResourceManager::CreateAsset<Texture2D>(singlePixelDescriptor);
		solidWhiteTex->LoadData(1, 1, PixelFormat::RGB, PixelType::Float, solidWhite);

#pragma endregion 

#pragma region Material Creation
		// Loading in a 1D LUT
		Texture1D::Sptr toonLut = ResourceManager::CreateAsset<Texture1D>("luts/toon-1D.png"); 
		toonLut->SetWrap(WrapMode::ClampToEdge);

		// Here we'll load in the cubemap, as well as a special shader to handle drawing the skybox
		TextureCube::Sptr testCubemap = ResourceManager::CreateAsset<TextureCube>("cubemaps/ocean/ocean.jpg");
		ShaderProgram::Sptr      skyboxShader = ResourceManager::CreateAsset<ShaderProgram>(std::unordered_map<ShaderPartType, std::string>{
			{ ShaderPartType::Vertex, "shaders/vertex_shaders/skybox_vert.glsl" },
			{ ShaderPartType::Fragment, "shaders/fragment_shaders/skybox_frag.glsl" } 
		});
		  
		// Create an empty scene
		Scene::Sptr scene = std::make_shared<Scene>();  

		// Setting up our enviroment map
		scene->SetSkyboxTexture(testCubemap); 
		scene->SetSkyboxShader(skyboxShader);
		// Since the skybox I used was for Y-up, we need to rotate it 90 deg around the X-axis to convert it to z-up 
		scene->SetSkyboxRotation(glm::rotate(MAT4_IDENTITY, glm::half_pi<float>(), glm::vec3(1.0f, 0.0f, 0.0f)));

		// Loading in a color lookup table
		Texture3D::Sptr lut = ResourceManager::CreateAsset<Texture3D>("luts/cool.CUBE");   

		// Configure the color correction LUT
		scene->SetColorLUT(lut);

		// Create our materials
		// This will be our box material, with no environment reflections
		Material::Sptr boxMaterial = ResourceManager::CreateAsset<Material>(deferredForward);
		{
			boxMaterial->Name = "Box";
			boxMaterial->Set("u_Material.AlbedoMap", boxTexture);
			boxMaterial->Set("u_Material.Shininess", 0.1f);
			boxMaterial->Set("u_Material.NormalMap", normalMapDefault);
		}

		// This will be the reflective material, we'll make the whole thing 90% reflective
		Material::Sptr monkeyMaterial = ResourceManager::CreateAsset<Material>(deferredForward);
		{
			monkeyMaterial->Name = "Monkey";
			monkeyMaterial->Set("u_Material.AlbedoMap", monkeyTex);
			monkeyMaterial->Set("u_Material.NormalMap", normalMapDefault);
			monkeyMaterial->Set("u_Material.Shininess", 0.5f);
		}

		// This will be the reflective material, we'll make the whole thing 50% reflective
		Material::Sptr testMaterial = ResourceManager::CreateAsset<Material>(deferredForward); 
		{
			testMaterial->Name = "Box-Specular";
			testMaterial->Set("u_Material.AlbedoMap", boxTexture); 
			testMaterial->Set("u_Material.Specular", boxSpec);
			testMaterial->Set("u_Material.NormalMap", normalMapDefault);
		}

		// Our foliage vertex shader material 
		Material::Sptr foliageMaterial = ResourceManager::CreateAsset<Material>(foliageShader);
		{
			foliageMaterial->Name = "Foliage Shader";
			foliageMaterial->Set("u_Material.AlbedoMap", leafTex);
			foliageMaterial->Set("u_Material.Shininess", 0.1f);
			foliageMaterial->Set("u_Material.DiscardThreshold", 0.1f);
			foliageMaterial->Set("u_Material.NormalMap", normalMapDefault);

			foliageMaterial->Set("u_WindDirection", glm::vec3(1.0f, 1.0f, 0.0f));
			foliageMaterial->Set("u_WindStrength", 0.5f);
			foliageMaterial->Set("u_VerticalScale", 1.0f);
			foliageMaterial->Set("u_WindSpeed", 1.0f);
		}

		// Our toon shader material
		Material::Sptr toonMaterial = ResourceManager::CreateAsset<Material>(celShader);
		{
			toonMaterial->Name = "Toon"; 
			toonMaterial->Set("u_Material.AlbedoMap", boxTexture);
			toonMaterial->Set("u_Material.NormalMap", normalMapDefault);
			toonMaterial->Set("s_ToonTerm", toonLut);
			toonMaterial->Set("u_Material.Shininess", 0.1f); 
			toonMaterial->Set("u_Material.Steps", 8);
		}


		Material::Sptr displacementTest = ResourceManager::CreateAsset<Material>(displacementShader);
		{
			Texture2D::Sptr displacementMap = ResourceManager::CreateAsset<Texture2D>("textures/displacement_map.png");
			Texture2D::Sptr normalMap       = ResourceManager::CreateAsset<Texture2D>("textures/normal_map.png");
			Texture2D::Sptr diffuseMap      = ResourceManager::CreateAsset<Texture2D>("textures/bricks_diffuse.png");

			displacementTest->Name = "Displacement Map";
			displacementTest->Set("u_Material.AlbedoMap", diffuseMap);
			displacementTest->Set("u_Material.NormalMap", normalMap);
			displacementTest->Set("s_Heightmap", displacementMap);
			displacementTest->Set("u_Material.Shininess", 0.5f);
			displacementTest->Set("u_Scale", 0.1f);
		}

		Material::Sptr normalmapMat = ResourceManager::CreateAsset<Material>(deferredForward);
		{
			Texture2D::Sptr normalMap       = ResourceManager::CreateAsset<Texture2D>("textures/normal_map.png");
			Texture2D::Sptr diffuseMap      = ResourceManager::CreateAsset<Texture2D>("textures/bricks_diffuse.png");

			normalmapMat->Name = "Tangent Space Normal Map";
			normalmapMat->Set("u_Material.AlbedoMap", diffuseMap);
			normalmapMat->Set("u_Material.NormalMap", normalMap);
			normalmapMat->Set("u_Material.Shininess", 0.5f);
			normalmapMat->Set("u_Scale", 0.1f);
		}

		Material::Sptr multiTextureMat = ResourceManager::CreateAsset<Material>(multiTextureShader);
		{
			Texture2D::Sptr sand  = ResourceManager::CreateAsset<Texture2D>("textures/terrain/sand.png");
			Texture2D::Sptr grass = ResourceManager::CreateAsset<Texture2D>("textures/terrain/grass.png");

			multiTextureMat->Name = "Multitexturing";
			multiTextureMat->Set("u_Material.DiffuseA", sand);
			multiTextureMat->Set("u_Material.DiffuseB", grass);
			multiTextureMat->Set("u_Material.NormalMapA", normalMapDefault);
			multiTextureMat->Set("u_Material.NormalMapB", normalMapDefault);
			multiTextureMat->Set("u_Material.Shininess", 0.5f);
			multiTextureMat->Set("u_Scale", 0.1f); 
		}

		//Our previous materials
		Material::Sptr gardenTowerMaterial = ResourceManager::CreateAsset<Material>(deferredForward);
		{
			gardenTowerMaterial->Set("u_Material.AlbedoMap", gardenTowerTexture);
			gardenTowerMaterial->Set("u_Material.Shininess", 0.1f);
			gardenTowerMaterial->Set("u_Material.NormalMap", normalMapDefault);
		}

		Material::Sptr cannonBallMaterial = ResourceManager::CreateAsset<Material>(deferredForward);
		{
			cannonBallMaterial->Set("u_Material.AlbedoMap", boxTexture);
			cannonBallMaterial->Set("u_Material.Shininess", 0.1f);
			cannonBallMaterial->Set("u_Material.NormalMap", normalMapDefault);
		}

		Material::Sptr goblinMaterial = ResourceManager::CreateAsset<Material>(deferredForward);
		{
			goblinMaterial->Set("u_Material.AlbedoMap", goblinTex);
			goblinMaterial->Set("u_Material.Shininess", 0.1f);
			goblinMaterial->Set("u_Material.NormalMap", normalMapDefault);
		}

		Material::Sptr newGoblinMaterial = ResourceManager::CreateAsset<Material>(deferredForward);
		{
			newGoblinMaterial->Set("u_Material.AlbedoMap", goblinTex);
			newGoblinMaterial->Set("u_Material.Shininess", 0.1f);
			newGoblinMaterial->Set("u_Material.NormalMap", normalMapDefault);
		}

		//Our new materials
		Material::Sptr winterGardenMaterial = ResourceManager::CreateAsset<Material>(deferredForward);
		{
			winterGardenMaterial->Name = "Winter Garden Mat";
			winterGardenMaterial->Set("u_Material.AlbedoMap", winterGardenTexture);
			winterGardenMaterial->Set("u_Material.Shininess", 0.1f);
			winterGardenMaterial->Set("u_Material.NormalMap", normalMapDefault);
		}

		//frame 1 material stuff
		Material::Sptr birdFlyMaterial = ResourceManager::CreateAsset<Material>(deferredForward);
		{
			birdFlyMaterial->Name = "birdFly Mat";
			birdFlyMaterial->Set("u_Material.AlbedoMap", birdTexture);
			birdFlyMaterial->Set("u_Material.Shininess", 0.1f);
			birdFlyMaterial->Set("u_Material.NormalMap", normalMapDefault);
		}

		Material::Sptr goblinAttackMaterial = ResourceManager::CreateAsset<Material>(deferredForward);
		{
			goblinAttackMaterial->Name = "goblinAttack Mat";
			goblinAttackMaterial->Set("u_Material.AlbedoMap", goblinAttackTexture);
			goblinAttackMaterial->Set("u_Material.Shininess", 0.1f);
			goblinAttackMaterial->Set("u_Material.NormalMap", normalMapDefault);
		}

		Material::Sptr oozeMaterial = ResourceManager::CreateAsset<Material>(deferredForward);
		{
			oozeMaterial->Name = "ooze Mat";
			oozeMaterial->Set("u_Material.AlbedoMap", oozeWalkTexture);
			oozeMaterial->Set("u_Material.Shininess", 0.1f);
			oozeMaterial->Set("u_Material.NormalMap", normalMapDefault);
		}

		Material::Sptr zombieAttackMaterial = ResourceManager::CreateAsset<Material>(deferredForward);
		{
			zombieAttackMaterial->Name = "zombieAttack Mat";
			zombieAttackMaterial->Set("u_Material.AlbedoMap", zombieTexture);
			zombieAttackMaterial->Set("u_Material.Shininess", 0.1f);
			zombieAttackMaterial->Set("u_Material.NormalMap", normalMapDefault);
		}


		//Cannon stuff
		Material::Sptr cannonBaseMaterial = ResourceManager::CreateAsset<Material>(deferredForward);
		{
			cannonBaseMaterial->Name = "cannonBase mate";
			cannonBaseMaterial->Set("u_Material.AlbedoMap", cannonBaseTexture);
			cannonBaseMaterial->Set("u_Material.Shininess", 0.1f);
			cannonBaseMaterial->Set("u_Material.NormalMap", normalMapDefault);
		}

		Material::Sptr cannonBarrelMaterial = ResourceManager::CreateAsset<Material>(deferredForward);
		{
			cannonBarrelMaterial->Name = "cannon Barrel mat";
			cannonBarrelMaterial->Set("u_Material.AlbedoMap", cannonBarrelTexture);
			cannonBarrelMaterial->Set("u_Material.Shininess", 0.1f);
			cannonBarrelMaterial->Set("u_Material.NormalMap", normalMapDefault);
		}


#pragma endregion

#pragma region Lights Camera Action
		// Create some lights for our scene
		GameObject::Sptr lightParent = scene->CreateGameObject("Lights");

		GameObject::Sptr mainLight = scene->CreateGameObject("mainLight");
		mainLight->SetPostion(glm::vec3(0.0f,0.0f,10.0f));
		lightParent->AddChild(mainLight);

		Light::Sptr lightComponent = mainLight->Add<Light>();
		lightComponent->SetColor(glm::vec3(1.0f,1.0f,1.0f));
		lightComponent->SetRadius(50.0f); //25
		lightComponent->SetIntensity(200.0f); //25

		//Additional lights randomized. Default 50.
		for (int ix = 0; ix < 0; ix++) {
			GameObject::Sptr light = scene->CreateGameObject("Light");
			light->SetPostion(glm::vec3(glm::diskRand(25.0f), 1.0f));
			lightParent->AddChild(light);

			Light::Sptr lightComponent = light->Add<Light>();
			lightComponent->SetColor(glm::linearRand(glm::vec3(0.0f), glm::vec3(1.0f)));
			lightComponent->SetRadius(glm::linearRand(0.1f, 10.0f));
			lightComponent->SetIntensity(glm::linearRand(1.0f, 2.0f));
		}

		// Set up the scene's camera
		GameObject::Sptr camera = scene->MainCamera->GetGameObject()->SelfRef();
		{
			camera->SetPostion({ 2.75, 0, 5 }); //-9,-6,15 ; 2.75, 0, 5 
			camera->SetRotation(glm::vec3(50.0f,0.f,-90.0f)); //90, 0,0 
			//camera->LookAt(glm::vec3(0.0f));

			//Need to create a camera controller for gameplay
			//camera->Add<CameraVanguard>();
			 
			//camera->Add<SimpleCameraControl>();


			// This is now handled by scene itself!
			//Camera::Sptr cam = camera->Add<Camera>();
			// Make sure that the camera is set as the scene's main camera!
			//scene->MainCamera = cam;
		}

#pragma endregion

#pragma region Setting the Scene
		// We'll create a mesh that is a simple plane that we can resize later
		MeshResource::Sptr planeMesh = ResourceManager::CreateAsset<MeshResource>();
		planeMesh->AddParam(MeshBuilderParam::CreatePlane(ZERO, UNIT_Z, UNIT_X, glm::vec2(1.0f)));
		planeMesh->GenerateMesh();

		MeshResource::Sptr sphere = ResourceManager::CreateAsset<MeshResource>();
		sphere->AddParam(MeshBuilderParam::CreateIcoSphere(ZERO, ONE, 5));
		sphere->GenerateMesh();

		//Parents for organization
		GameObject::Sptr defaultsParent = scene->CreateGameObject("Defaults"); {
			defaultsParent->SetPostion(glm::vec3(0.0f,0.0f,0.0f));
		}
		GameObject::Sptr mapParent = scene->CreateGameObject("Map"); {

		}

		GameObject::Sptr cameraOffset = scene->CreateGameObject("Camera Offset"); {
			cameraOffset->Add<CameraVanguard>();
		}

		GameObject::Sptr gameObjectsParent = scene->CreateGameObject("Game Objects");
		GameObject::Sptr enemiesParent = scene->CreateGameObject("Enemies");
		GameObject::Sptr uiParent = scene->CreateGameObject("UI");
		GameObject::Sptr cannonParent = scene->CreateGameObject("CannonParts");

		cameraOffset->AddChild(camera);
		gameObjectsParent->AddChild(enemiesParent);
		gameObjectsParent->AddChild(cannonParent);
		

		// Set up all our sample objects
		GameObject::Sptr plane = scene->CreateGameObject("Plane");
		{
			plane->SetPostion(glm::vec3(0.0f,0.0f,-4.0f));

			// Make a big tiled mesh
			MeshResource::Sptr tiledMesh = ResourceManager::CreateAsset<MeshResource>();
			tiledMesh->AddParam(MeshBuilderParam::CreatePlane(ZERO, UNIT_Z, UNIT_X, glm::vec2(100.0f), glm::vec2(20.0f)));
			tiledMesh->GenerateMesh();

			// Create and attach a RenderComponent to the object to draw our mesh
			RenderComponent::Sptr renderer = plane->Add<RenderComponent>();
			renderer->SetMesh(tiledMesh);
			renderer->SetMaterial(boxMaterial);

			// Attach a plane collider that extends infinitely along the X/Y axis
			RigidBody::Sptr physics = plane->Add<RigidBody>(/*static by default*/);
			physics->AddCollider(BoxCollider::Create(glm::vec3(50.0f, 50.0f, 1.0f)))->SetPosition({ 0,0,-1 });

			defaultsParent->AddChild(plane);
		}


		GameObject::Sptr WinterGarden = scene->CreateGameObject("Winter Garden");
		{
			// Set position in the scene
			WinterGarden->SetPostion(glm::vec3(0.0f, 0.0f, 0.0f));
			WinterGarden->SetRotation(glm::vec3(90.0f, 0.0f, 0.0f));
			WinterGarden->SetScale(glm::vec3(0.100f,0.100f,0.100f));

			RenderComponent::Sptr renderer = WinterGarden->Add<RenderComponent>();
			renderer->SetMesh(winterGardenMesh);
			renderer->SetMaterial(winterGardenMaterial);

			mapParent->AddChild(WinterGarden);
		}


		GameObject::Sptr towerGarden = scene->CreateGameObject("towerGarden");
		{
			// Set position in the scene
			towerGarden->SetPostion(glm::vec3(-130.69f, -143.80f, -400.0f)); //-130.69, -143.80, -4
			towerGarden->SetRotation(glm::vec3(90.0f, 0.0f, 0.0f));

			RenderComponent::Sptr renderer = towerGarden->Add<RenderComponent>();
			renderer->SetMesh(towerGardenMesh);
			renderer->SetMaterial(gardenTowerMaterial);
			
			mapParent->AddChild(towerGarden);
		}

		GameObject::Sptr cannonBall = scene->CreateGameObject("cannonBall");
		{
			cannonBall->SetPostion(glm::vec3(12.6f, -10.4f, 1.0f));
			cannonBall->SetRotation(glm::vec3(90.0f, 0.0f, 0.0f));
			cannonBall->SetScale(glm::vec3(1.f));

			//Add a rigidbody to hit with force
			RigidBody::Sptr ballPhy = cannonBall->Add<RigidBody>(RigidBodyType::Dynamic);
			ballPhy->SetMass(5.0f);
			ballPhy->AddCollider(SphereCollider::Create(1.f))->SetPosition({ 0, 0, 0 });

			/*TriggerVolume::Sptr volume = cannonBall->Add<TriggerVolume>();
			  SphereCollider::Sptr collider = SphereCollider::Create(1.f);
			  collider->SetPosition(glm::vec3(0.f));
			  volume->AddCollider(collider);

			  cannonBall->Add<TriggerVolumeEnterBehaviour>();*/

			  // Create and attach a renderer for the monkey
			RenderComponent::Sptr renderer = cannonBall->Add<RenderComponent>();
			renderer->SetMesh(cannonBallMesh);
			renderer->SetMaterial(cannonBallMaterial);

			gameObjectsParent->AddChild(cannonBall);
		}

		GameObject::Sptr cannonBarrel = scene->CreateGameObject("Cannon Barrel"); {
			cannonBarrel->SetPostion(glm::vec3(12.6f, -10.4f, 1.0f));
			cannonBarrel->SetRotation(glm::vec3(90.0f, 0.0f, 0.0f));
			cannonBarrel->SetScale(glm::vec3(1.f));

			RenderComponent::Sptr renderer = cannonBarrel->Add<RenderComponent>();
			renderer->SetMesh(cannonBarrelMesh);
			renderer->SetMaterial(cannonBarrelMaterial); //needs

			cannonParent->AddChild(cannonBarrel);
		};

		GameObject::Sptr cannonBase = scene->CreateGameObject("Cannon Base"); {
			cannonBase->SetPostion(glm::vec3(12.6f, -10.4f, 1.0f));
			cannonBase->SetRotation(glm::vec3(90.0f, 0.0f, 0.0f));
			cannonBase->SetScale(glm::vec3(1.f));

			RenderComponent::Sptr renderer = cannonBase->Add<RenderComponent>();
			renderer->SetMesh(cannonBaseMesh);
			renderer->SetMaterial(cannonBaseMaterial); //needs

			cannonParent->AddChild(cannonBase);
		};

		GameObject::Sptr towerCannon = scene->CreateGameObject("towerCannon");
		{
			towerCannon->SetPostion(glm::vec3(0.0f, 0.0f, 0.0f));
			towerCannon->SetRotation(glm::vec3(90.0f, 0.0f, 0.0f));

			// Add some behaviour that relies on the physics body
			//towerGarden->Add<JumpBehaviour>();

			// Create and attach a renderer for the monkey
			RenderComponent::Sptr renderer = towerCannon->Add<RenderComponent>();
			renderer->SetMesh(towerCannonMesh);
			renderer->SetMaterial(gardenTowerMaterial);
			mapParent->AddChild(towerCannon);
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

			mapParent->AddChild(towerSpears);

		}


		GameObject::Sptr goblin1 = scene->CreateGameObject("goblin1");
		{
			// Set position in the scene
			goblin1->SetPostion(glm::vec3(12.760f, 0.0f, 1.0f));
			goblin1->SetRotation(glm::vec3(90.0f, 0.0f, -90.0f));
			goblin1->SetScale(glm::vec3(2.0f));

			// Add some behaviour that relies on the physics body
			//towerGarden->Add<JumpBehaviour>();

			// Create and attach a renderer for the monkey
			RenderComponent::Sptr renderer = goblin1->Add<RenderComponent>();
			renderer->SetMesh(newGoblinMesh);
			renderer->SetMaterial(goblinMaterial);

			//RigidBody::Sptr goblinRB = goblin1->Add<RigidBody>(RigidBodyType::Dynamic);
			//goblinRB->AddCollider(BoxCollider::Create())->SetPosition(glm::vec3(0.f));

			TriggerVolume::Sptr volume = goblin1->Add<TriggerVolume>();
			CylinderCollider::Sptr col = CylinderCollider::Create(glm::vec3(1.f, 1.f, 1.f));
			volume->AddCollider(col);

			goblin1->Add<TriggerVolumeEnterBehaviour>();
			goblin1->Add<EnemyMovement>();
			

			// Add a dynamic rigid body to this monkey
			RigidBody::Sptr physics = goblin1->Add<RigidBody>(RigidBodyType::Dynamic);
			physics->AddCollider(ConvexMeshCollider::Create());

			enemiesParent->AddChild(goblin1);

		}

		//Frame 1 stuff and more stuff
		GameObject::Sptr birdFly = scene->CreateGameObject("birdFly"); {
			birdFly->SetPostion(glm::vec3(10, 5.f, 5.0f));
			birdFly->SetRotation(glm::vec3(90.0f, 145.0f, 96.0f));
			birdFly->SetScale(glm::vec3(1.f));

			RenderComponent::Sptr renderer = birdFly->Add<RenderComponent>();
			renderer->SetMesh(birdFlyMesh);
			renderer->SetMaterial(birdFlyMaterial); //needs

			enemiesParent->AddChild(birdFly);
		};

		GameObject::Sptr goblinAttack = scene->CreateGameObject("goblinAttack"); {
			goblinAttack->SetPostion(glm::vec3(7.62f,-2.97f, 1.0f));
			goblinAttack->SetRotation(glm::vec3(90.0f, 0.0f, -90.0f));
			goblinAttack->SetScale(glm::vec3(1.f));

			RenderComponent::Sptr renderer = goblinAttack->Add<RenderComponent>();
			renderer->SetMesh(goblinAttackMesh);
			renderer->SetMaterial(goblinAttackMaterial); //needs

			enemiesParent->AddChild(goblinAttack);
		};

		GameObject::Sptr oozeWalk = scene->CreateGameObject("oozeWalk"); {
			oozeWalk->SetPostion(glm::vec3(5, 0.f, 2.0f));
			oozeWalk->SetRotation(glm::vec3(90.0f, 0.0f, 0.0f));
			oozeWalk->SetScale(glm::vec3(1.f));

			RenderComponent::Sptr renderer = oozeWalk->Add<RenderComponent>();
			renderer->SetMesh(oozeMesh);
			renderer->SetMaterial(oozeMaterial); //needs

			enemiesParent->AddChild(oozeWalk);
		};

		GameObject::Sptr zombieAttack = scene->CreateGameObject("zombieAttack"); {
			zombieAttack->SetPostion(glm::vec3(6.70, 2.970f, 2.0f));
			zombieAttack->SetRotation(glm::vec3(90.0f, 0.0f, -90.0f));
			zombieAttack->SetScale(glm::vec3(1.f));

			RenderComponent::Sptr renderer = zombieAttack->Add<RenderComponent>();
			renderer->SetMesh(zombieAttackMesh);
			renderer->SetMaterial(zombieAttackMaterial); //needs

			enemiesParent->AddChild(zombieAttack);
		};
		

#pragma endregion

#pragma region UI creation

		//pain

/*
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

			uiParent->AddChild(canvas);
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
			uiParent->AddChild(canvas2);
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
			uiParent->AddChild(canvas3);
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
			uiParent->AddChild(canvas4);
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
			uiParent->AddChild(canvas5);
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
			uiParent->AddChild(canvas6);
		}



		*/
#pragma endregion

#pragma region Commented Defaults
	/*
		GameObject::Sptr monkey1 = scene->CreateGameObject("Monkey 1");
		{
			// Set position in the scene
			monkey1->SetPostion(glm::vec3(1.5f, 0.0f, 1.0f));

			// Add some behaviour that relies on the physics body
			monkey1->Add<JumpBehaviour>();

			// Create and attach a renderer for the monkey
			RenderComponent::Sptr renderer = monkey1->Add<RenderComponent>();
			renderer->SetMesh(monkeyMesh);
			renderer->SetMaterial(monkeyMaterial);

			// Example of a trigger that interacts with static and kinematic bodies as well as dynamic bodies
			TriggerVolume::Sptr trigger = monkey1->Add<TriggerVolume>();
			trigger->SetFlags(TriggerTypeFlags::Statics | TriggerTypeFlags::Kinematics);
			trigger->AddCollider(BoxCollider::Create(glm::vec3(1.0f)));

			monkey1->Add<TriggerVolumeEnterBehaviour>();
		}


		GameObject::Sptr ship = scene->CreateGameObject("Fenrir");
		{
			// Set position in the scene
			ship->SetPostion(glm::vec3(1.5f, 0.0f, 4.0f));
			ship->SetScale(glm::vec3(0.1f));

			// Create and attach a renderer for the monkey
			RenderComponent::Sptr renderer = ship->Add<RenderComponent>();
			renderer->SetMesh(shipMesh);
			renderer->SetMaterial(monkeyMaterial);
		}

		GameObject::Sptr demoBase = scene->CreateGameObject("Demo Parent");

		// Box to showcase the specular material
		GameObject::Sptr specBox = scene->CreateGameObject("Specular Object");
		{
			MeshResource::Sptr boxMesh = ResourceManager::CreateAsset<MeshResource>();
			boxMesh->AddParam(MeshBuilderParam::CreateCube(ZERO, ONE));
			boxMesh->GenerateMesh();

			// Set and rotation position in the scene
			specBox->SetPostion(glm::vec3(0, -4.0f, 1.0f));

			// Add a render component
			RenderComponent::Sptr renderer = specBox->Add<RenderComponent>();
			renderer->SetMesh(boxMesh);
			renderer->SetMaterial(testMaterial); 

			demoBase->AddChild(specBox);
		}

		// sphere to showcase the foliage material
		GameObject::Sptr foliageBall = scene->CreateGameObject("Foliage Sphere");
		{
			// Set and rotation position in the scene
			foliageBall->SetPostion(glm::vec3(-4.0f, -4.0f, 1.0f));

			// Add a render component
			RenderComponent::Sptr renderer = foliageBall->Add<RenderComponent>();
			renderer->SetMesh(sphere);
			renderer->SetMaterial(foliageMaterial);

			demoBase->AddChild(foliageBall);
		}

		// Box to showcase the foliage material
		GameObject::Sptr foliageBox = scene->CreateGameObject("Foliage Box");
		{
			MeshResource::Sptr box = ResourceManager::CreateAsset<MeshResource>();
			box->AddParam(MeshBuilderParam::CreateCube(glm::vec3(0, 0, 0.5f), ONE));
			box->GenerateMesh();

			// Set and rotation position in the scene
			foliageBox->SetPostion(glm::vec3(-6.0f, -4.0f, 1.0f));

			// Add a render component
			RenderComponent::Sptr renderer = foliageBox->Add<RenderComponent>();
			renderer->SetMesh(box);
			renderer->SetMaterial(foliageMaterial);

			demoBase->AddChild(foliageBox);
		}

		// Box to showcase the specular material
		GameObject::Sptr toonBall = scene->CreateGameObject("Toon Object");
		{
			// Set and rotation position in the scene
			toonBall->SetPostion(glm::vec3(-2.0f, -4.0f, 1.0f));

			// Add a render component
			RenderComponent::Sptr renderer = toonBall->Add<RenderComponent>();
			renderer->SetMesh(sphere);
			renderer->SetMaterial(toonMaterial);

			demoBase->AddChild(toonBall);
		}

		GameObject::Sptr displacementBall = scene->CreateGameObject("Displacement Object");
		{
			// Set and rotation position in the scene
			displacementBall->SetPostion(glm::vec3(2.0f, -4.0f, 1.0f));

			// Add a render component
			RenderComponent::Sptr renderer = displacementBall->Add<RenderComponent>();
			renderer->SetMesh(sphere);
			renderer->SetMaterial(displacementTest);

			demoBase->AddChild(displacementBall);
		}

		GameObject::Sptr multiTextureBall = scene->CreateGameObject("Multitextured Object");
		{
			// Set and rotation position in the scene 
			multiTextureBall->SetPostion(glm::vec3(4.0f, -4.0f, 1.0f));

			// Add a render component 
			RenderComponent::Sptr renderer = multiTextureBall->Add<RenderComponent>();
			renderer->SetMesh(sphere);
			renderer->SetMaterial(multiTextureMat);

			demoBase->AddChild(multiTextureBall);
		}

		GameObject::Sptr normalMapBall = scene->CreateGameObject("Normal Mapped Object");
		{
			// Set and rotation position in the scene 
			normalMapBall->SetPostion(glm::vec3(6.0f, -4.0f, 1.0f));

			// Add a render component 
			RenderComponent::Sptr renderer = normalMapBall->Add<RenderComponent>();
			renderer->SetMesh(sphere);
			renderer->SetMaterial(normalmapMat);

			demoBase->AddChild(normalMapBall);
		}

		// Create a trigger volume for testing how we can detect collisions with objects!
		GameObject::Sptr trigger = scene->CreateGameObject("Trigger");
		{
			TriggerVolume::Sptr volume = trigger->Add<TriggerVolume>();
			CylinderCollider::Sptr collider = CylinderCollider::Create(glm::vec3(3.0f, 3.0f, 1.0f));
			collider->SetPosition(glm::vec3(0.0f, 0.0f, 0.5f));
			volume->AddCollider(collider);

			trigger->Add<TriggerVolumeEnterBehaviour>();
		}

		/////////////////////////// UI //////////////////////////////
		/*
		GameObject::Sptr canvas = scene->CreateGameObject("UI Canvas");
		{
			RectTransform::Sptr transform = canvas->Add<RectTransform>();
			transform->SetMin({ 16, 16 });
			transform->SetMax({ 256, 256 });

			GuiPanel::Sptr canPanel = canvas->Add<GuiPanel>();


			GameObject::Sptr subPanel = scene->CreateGameObject("Sub Item");
			{
				RectTransform::Sptr transform = subPanel->Add<RectTransform>();
				transform->SetMin({ 10, 10 });
				transform->SetMax({ 128, 128 });

				GuiPanel::Sptr panel = subPanel->Add<GuiPanel>();
				panel->SetColor(glm::vec4(1.0f, 0.0f, 0.0f, 1.0f));

				panel->SetTexture(ResourceManager::CreateAsset<Texture2D>("textures/upArrow.png"));

				Font::Sptr font = ResourceManager::CreateAsset<Font>("fonts/Roboto-Medium.ttf", 16.0f);
				font->Bake();

				GuiText::Sptr text = subPanel->Add<GuiText>();
				text->SetText("Hello world!");
				text->SetFont(font);

				monkey1->Get<JumpBehaviour>()->Panel = text;
			}

			canvas->AddChild(subPanel);
		}
		

		GameObject::Sptr particles = scene->CreateGameObject("Particles");
		{
			ParticleSystem::Sptr particleManager = particles->Add<ParticleSystem>();  
			particleManager->AddEmitter(glm::vec3(0.0f), glm::vec3(0.0f, -1.0f, 10.0f), 10.0f, glm::vec4(0.0f, 1.0f, 0.0f, 1.0f)); 
		}

	*/

#pragma endregion

		GuiBatcher::SetDefaultTexture(ResourceManager::CreateAsset<Texture2D>("textures/ui-sprite.png"));
		GuiBatcher::SetDefaultBorderRadius(8);

		// Save the asset manifest for all the resources we just loaded
		ResourceManager::SaveManifest("scene-manifest.json");
		// Save the scene to a JSON file
		scene->Save("scene.json");

		// Send the scene to the application
		app.LoadScene(scene);
	}
}
