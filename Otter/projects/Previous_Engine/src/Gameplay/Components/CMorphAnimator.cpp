/*
NOU Framework - Created for INFR 2310 at Ontario Tech.
(c) Samantha Stahlke 2020

CMorphAnimator.cpp
Simple animator component for demonstrating morph target animation.

As a convention in NOU, we put "C" before a class name to signify
that we intend the class for use as a component with the ENTT framework.
*/

#include "CMorphAnimator.h"
#include "CMorphMeshRenderer.h"
#include "NOU/Mesh.h"


namespace nou
{
	CMorphAnimator::AnimData::AnimData()
	{
		frame0 = nullptr;
		frame1 = nullptr;
		frameTime = 0.5f;
		currentFrame = 0.0f;
	}


	CMorphAnimator::CMorphAnimator(Entity& owner)
	{
		m_owner = &owner;
		m_data = std::make_unique<AnimData>();
		m_timer = 0.0f;
		m_forwards = true;
	}

	
	void CMorphAnimator::Update(float deltaTime)
	{
		// TODO: Complete this function
		m_timer += deltaTime * 2;

		if (m_data->currentFrame == m_data->frames.size()-1)
		{
			m_owner->Get<CMorphMeshRenderer>().UpdateData(*m_data->frames[m_data->currentFrame], *m_data->frames[0.0f], m_timer);
		}
		else
		{
			m_owner->Get<CMorphMeshRenderer>().UpdateData(*m_data->frames[m_data->currentFrame], *m_data->frames[m_data->currentFrame + 1.0f], m_timer);
		}

		if (m_timer >= m_data->frameTime * 2)
		{			
			if (m_data->currentFrame == m_data->frames.size() - 1)
			{
				m_data->currentFrame = 0;
			}
			else
			{
				m_data->currentFrame += 1.0f;
			}
			m_timer = 0;
		}
	}

	void CMorphAnimator::SetFrameTime(float time)
	{
		m_data->frameTime = time;
	}

	void CMorphAnimator::SetFrames(std::vector<std::unique_ptr<Mesh>> tempFrames)
	{
		for (int i = 0; i < tempFrames.size(); i++)
		{
			m_data->frames.push_back(std::move(tempFrames[i]));
		}

	}


}