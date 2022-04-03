#pragma once
#include "IComponent.h"

struct GLFWwindow;

/// <summary>
/// Camera rotates upon hiting A or D keys. Right now its janky 90 degree turns with no smoothing or key press delay. Press lightly.
///
/// </summary>
class CameraVanguard : public Gameplay::IComponent {
public:
	typedef std::shared_ptr<CameraVanguard> Sptr;

	CameraVanguard();
	virtual ~CameraVanguard();

	virtual void Update(float deltaTime) override;

public:
	virtual void RenderImGui() override;
	MAKE_TYPENAME(CameraVanguard);
	virtual nlohmann::json ToJson() const override;
	static CameraVanguard::Sptr FromJson(const nlohmann::json& blob);

protected:
	//float _shiftMultipler;
	//glm::vec2 _mouseSensitivity;
	//glm::vec3 _moveSpeeds;
	//glm::dvec2 _prevMousePos;
	//glm::vec2 _currentRot;
	glm::vec3 _cameraRotation;
	//int _cameraIteration;
	//bool _isMousePressed = false;
};