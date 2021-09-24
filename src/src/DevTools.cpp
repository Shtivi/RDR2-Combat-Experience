#include "Main.h"

bool DevTools::active = false;

bool DevTools::isActive()
{
	return active;
}

void DevTools::toggle()
{
	if (active)
	{
		disable();
	}
	else
	{
		enable();
	}
}

void DevTools::enable()
{
	showSubtitle("Debug tools ON");
	ENTITY::SET_ENTITY_INVINCIBLE(player, true);
	active = true;
}

void DevTools::disable()
{
	showSubtitle("Debug tools OFF");
	ENTITY::SET_ENTITY_INVINCIBLE(player, false);
	active = false;
}

void DevTools::update()
{

	Vector3 pos = playerPos();

	Hash weaponHash;
	WEAPON::GET_CURRENT_PED_WEAPON(player, &weaponHash, 0, 0, 0);
	if (weaponHash != MISC::GET_HASH_KEY("WEAPON_UNARMED")) {
		Entity e;
		if (PLAYER::GET_ENTITY_PLAYER_IS_FREE_AIMING_AT(PLAYER::PLAYER_ID(), &e) /*&& distanceBetweenEntities(player, e) < 20*/) 
		{
			debug(PED::GET_PED_CONFIG_FLAG(e, 11, 0));
			if (IsKeyJustUp(VK_KEY_K))
			{
				showSubtitle(distance(e, player));
				if (distance(e, player) <= 15)
				{
					log(entityPos(e));
				}
			}

			if (IsKeyJustUp(VK_KEY_Z))
			{
				int bone = 0;
				PED::GET_PED_LAST_DAMAGE_BONE(e, &bone);
				showSubtitle(to_string( bone));
			}
		}
		else
		{
			if (IsKeyJustUp(VK_KEY_Z))
			{
				RaycastResult ray = raycastCrosshair(30, RaycastIntersectionOptions::Map);
				if (distance(ray.hitPos, player) < 30)
				{
					Vector3 spawnPos = getGroundPos(ray.hitPos);
					MISC::CLEAR_AREA(spawnPos.x, spawnPos.y, spawnPos.z, 5, 0);
					log(to_string(spawnPos));
					Propset::spawn(spawnPos, "pg_ambcamp01x_tent_ground02");
					// pg_ambcamp01x_tent_burlap
					// pg_ambcamp01x_tent_ground02
				}
			}
		}
	}
	else
	{
		Entity targetEntity;
		if (PLAYER::GET_PLAYER_TARGET_ENTITY(PLAYER::PLAYER_ID(), &targetEntity))
		{
			
			if (IsKeyJustUp(VK_KEY_Z))
			{
			}
		}
		else
		{

		}
	}

	if (IsKeyJustUp(VK_KEY_X))
	{

	}

	if (IsKeyJustUp(VK_F1))
	{

		Ped ped = createPed("a_m_m_htlroughtravellers_01", getGroundPos(playerPos() + getForwardVector(player) * 5));
		DECORATOR::DECOR_SET_INT(ped, "honor_override", 0);
		PED::SET_PED_CONFIG_FLAG(ped, 6, 1);
		/*EVENT::_0xBB1E41DD3D3C6250(ped, (Any)"SpDefaultStealth", 0);
		TASK::CLEAR_PED_TASKS_IMMEDIATELY(player, 1, 1);*/
	}

	LAW::_SET_WANTED_INTENSITY_FOR_PLAYER(PLAYER::PLAYER_ID(), 0);
	PLAYER::SET_MAX_WANTED_LEVEL(0);

	if (IsKeyJustUp(VK_KEY_Z))
	{
		showSubtitle(to_string(playerPos()));
		log(to_string(playerPos()));
	}


	if (IsKeyJustUp(VK_F3))
	{
	}



	if (IsKeyJustUp(VK_KEY_K))
	{
	}
}