#include "Gameplay/Components/CameraVanguard.h"
#include <GLFW/glfw3.h>
#define  GLM_SWIZZLE
#include <GLM/gtc/quaternion.hpp>

#include "Gameplay/GameObject.h"
#include "Gameplay/Scene.h"
#include "Utils/JsonGlmHelpers.h"
#include "Utils/ImGuiHelper.h"
#include "Gameplay/InputEngine.h"
#include "Application/Application.h"

CameraVanguard::CameraVanguard() :
	IComponent(),
	_cameraRotation(glm::vec3(0.0f, 0.0f, 0.0f))
{ }

CameraVanguard::~CameraVanguard() = default;

void CameraVanguard::Update(float deltaTime)
{
	if (Application::Get().IsFocused) {

		
			glm::vec3 input = glm::vec3(0, 0, 0.0f);
			glm::vec3 rotateCounter = glm::vec3(0,0,0);
			//glm::vec3 rotating = glm::vec3(0);
			bool rotating = false;
			bool dir = false; // false is left, true is right.

			if (InputEngine::IsKeyDown(GLFW_KEY_W)) {
		
			}
			if (InputEngine::IsKeyDown(GLFW_KEY_S)) {
			
			}

			if (InputEngine::IsKeyDown(GLFW_KEY_A)) {
				_cameraRotation += glm::vec3(0, 0, 90); // *delta time
			
			}
			if (InputEngine::IsKeyDown(GLFW_KEY_D)) {
				_cameraRotation -= glm::vec3(0, 0, 90);
		
			}

			GetGameObject()->SetRotation(GetGameObject()->GetRotation().x + _cameraRotation);
			
	}
}


void CameraVanguard::RenderImGui()
{
	//LABEL_LEFT(ImGui::DragFloat2, "Mouse Sensitivity", &_mouseSensitivity.x, 0.01f);
//	LABEL_LEFT(ImGui::DragFloat3, "Move Speed       ", &_moveSpeeds.x, 0.01f, 0.01f);
//	LABEL_LEFT(ImGui::DragFloat, "Shift Multiplier ", &_shiftMultipler, 0.01f, 1.0f);
}

nlohmann::json CameraVanguard::ToJson() const {
	return {
	//	{ "mouse_sensitivity", _mouseSensitivity },
	//	{ "move_speed", _moveSpeeds },
	//	{ "shift_mult", _shiftMultipler }
	};
}

CameraVanguard::Sptr CameraVanguard::FromJson(const nlohmann::json & blob) {
	CameraVanguard::Sptr result = std::make_shared<CameraVanguard>();
	//result->_mouseSensitivity = JsonGet(blob, "mouse_sensitivity", result->_mouseSensitivity);
//	result->_moveSpeeds = JsonGet(blob, "move_speed", result->_moveSpeeds);
//	result->_shiftMultipler = JsonGet(blob, "shift_mult", 2.0f);
	return result;
}
