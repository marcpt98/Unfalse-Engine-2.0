#include "Animation.h"
#include "Application.h"
#include "ModuleRenderer3D.h"
#include "FileSystem.h"
#include "GameObject.h"
#include "Transform.h"
#include "GnJSON.h"

#include "ResourceAnimation.h"
#include "ResourceAnimationManager.h"

#include "glew/include/glew.h"
#include "ImGui/imgui.h"

#include "ResourceAnimation.h"

Animation::Animation() : Component(), name("No name"), _resource(nullptr)
{
	type = ComponentType::ANIMATION;

	name = nullptr;
	path = nullptr;
	rootChannel = nullptr;
	currentanimation = nullptr;
	init = false;
	anim_time = 0;
	time = 0;
}

Animation::~Animation()
{
	if (_resource != nullptr)
	{
		App->resources->ReleaseResource(_resource->GetUID());
		_resource = nullptr;
	}
}

void Animation::InitAnimation()
{
	anim_channels.clear();

	// Get root gameobject with all channels
	rootChannel = _gameObject->children[1]->children[0];

	// Order gameobjects
	std::vector<GameObject*> channels;
	App->scene->PreorderGameObjects(rootChannel, channels);

	// Pass them to map
	for (int i = 0; i < channels.size(); ++i)
	{
		anim_channels[channels[i]->name] = channels[i];
	}

	// Splice (not yet) and add animations
	AddAnimations();
}

void Animation::Update()
{
	if (init == false)
	{
		InitAnimation();
		init = true;
	}

	// Animation time
	time += App->GetLastDt();
	anim_time = currentanimation->anim_TicksPerSecond * time;

	// Loop
	if (anim_time > currentanimation->anim_Duration)
	{
		time = 0;
	}

	PlayAnimation();
	
	Render();
}

void Animation::PlayAnimation()
{
	for (std::map<std::string, GameObject*>::iterator it = anim_channels.begin(); it != anim_channels.end(); it++)
	{
		// Get channel related with gameobject on list
		Channel channel = currentanimation->anim_Channels.find(it->first)->second;

		// Update Transforms
		Transform* transform = (Transform*)it->second->GetComponent(ComponentType::TRANSFORM);

		std::map<double, float3>::const_iterator pos_prev = channel.chan_PosKeys.lower_bound(anim_time);
		if (pos_prev != channel.chan_PosKeys.end())
		{
			transform->SetPosition(pos_prev->second);
		}

		std::map<double, Quat>::const_iterator rot_prev = channel.chan_RotKeys.lower_bound(anim_time);
		if (rot_prev != channel.chan_RotKeys.end())
		{
			transform->SetRotation(rot_prev->second);
		}

		std::map<double, float3>::const_iterator scl_prev = channel.chan_ScaleKeys.lower_bound(anim_time);
		if (scl_prev != channel.chan_ScaleKeys.end())
		{
			transform->SetScale(scl_prev->second);
		}

		// Update matrix
		float4x4 Transform = transform->GetGlobalTransform().Transposed();
		float tempTransform[16];
		memcpy(tempTransform, Transform.ptr(), 16 * sizeof(float));

		float4x4 newTransform;
		newTransform.Set(tempTransform);
		Transform = newTransform.Transposed();
		transform->SetGlobalTransform(Transform);
	}
}

void Animation::Render()
{
	for (std::map<std::string, GameObject*>::const_iterator it = anim_channels.begin(); it != anim_channels.end(); it++)
	{
		for (int i = 0; i < it->second->GetChildrenAmount(); i++)
		{
			float4x4 start = float4x4::zero;
			float4x4 end = float4x4::zero;

			start = it->second->GetTransform()->GetGlobalTransform();
			end = it->second->GetChildAt(i)->GetTransform()->GetGlobalTransform();

			float3 startpos = float3::zero;
			float3 endpos = float3::zero;

			float3 _position = float3::zero;
			float3 _scale = float3::zero;
			Quat _rotation = Quat::identity;

			start.Decompose(_position, _rotation, _scale);
			startpos = _position;

			end.Decompose(_position, _rotation, _scale);
			endpos = _position;

			App->renderer3D->DrawCustomRay(startpos, endpos);
		}
	}
}

void Animation::AddAnimations()
{
	ResourceAnimationManager* animationlist = (ResourceAnimationManager*)App->resources->CreateResource(App->resources->GenerateUID(), ResourceType::RESOURCE_ANIMATION_MANAGER);

	animationlist->AddAnimation(_resource->GetUID());

	currentanimation = _resource;
}

void Animation::Save(GnJSONArray& save_array)
{
	GnJSONObj save_object;

	save_object.AddInt("Type", type);
	save_object.AddInt("AnimationID", _resource->GetUID());

	save_array.AddObject(save_object);
}

void Animation::Load(GnJSONObj& load_object)
{
	int animationUID = load_object.GetInt("AnimationID");
	SetResourceUID(animationUID);
}

void Animation::SetResourceUID(uint UID)
{
	if (_resource != nullptr)
		App->resources->ReleaseResource(_resourceUID);

	_resourceUID = UID;
	_resource = (ResourceAnimation*)App->resources->RequestResource(_resourceUID);
}

Resource* Animation::GetResource(ResourceType type)
{
	return _resource;
}

void Animation::OnEditor()
{
	if (ImGui::CollapsingHeader("Animation", ImGuiTreeNodeFlags_DefaultOpen))
	{
		ImGui::Checkbox(" Enabled", &enabled);

		ImGui::Separator();
		ImGui::Spacing();

		if (_resource != nullptr)
		{
			std::string animationID = "Animation: ";
			animationID.append(_resource->name);
			ImGui::Button(animationID.c_str());

			ImGui::Spacing();

			ImGui::Text("Assets path: %s", _resource->assetsFile.c_str());
			ImGui::Text("Library path: %s", _resource->libraryFile.c_str());

			ImGui::Spacing();
			ImGui::Separator();
			ImGui::Spacing();

			ImGui::Text("Channels: %d Duration: %f", _resource->anim_NumChannels, _resource->anim_Duration);
			ImGui::Text("Ticks: %f", _resource->anim_TicksPerSecond);
			ImGui::Text("Current animation time: %d", anim_time);
		}
		else
		{
			ImGui::Button("Unknown Animation");
		}

		ImGui::Spacing();
		ImGui::Text("UID: %d", _resourceUID);
		ImGui::Spacing();
	}
}

