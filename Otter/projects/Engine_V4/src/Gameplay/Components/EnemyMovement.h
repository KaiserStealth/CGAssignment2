#pragma once
#include "IComponent.h"
#include "Gameplay/Physics/RigidBody.h"

/// <summary>
/// The movement script for all enemies, different damage and 
/// move speed can be applied for different enemies
/// </summary>
class EnemyMovement : public Gameplay::IComponent {
public:
	typedef std::shared_ptr<EnemyMovement> Sptr;

	std::weak_ptr<Gameplay::IComponent> Panel;

	EnemyMovement();
	virtual ~EnemyMovement();

	virtual void Awake() override;
	virtual void Update(float deltaTime) override;
	virtual void OnEnteredTrigger(const std::shared_ptr<Gameplay::Physics::TriggerVolume>& trigger) override;

public:
	virtual void RenderImGui() override;
	MAKE_TYPENAME(EnemyMovement);
	virtual nlohmann::json ToJson() const override;
	static EnemyMovement::Sptr FromJson(const nlohmann::json& blob);

protected:
	float _moveSpeed;
	float _damage;

	Gameplay::Physics::RigidBody::Sptr _body;
};

