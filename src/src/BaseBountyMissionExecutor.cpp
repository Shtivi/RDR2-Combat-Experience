#include "Main.h"

BaseBountyMissionExecutor::BaseBountyMissionExecutor(BountyMissionData* missionData, MapArea* area, BountyMissionStatus status)
{
	this->missionData = missionData;
	this->area = area;
	this->areaBlip = 0;
	this->targetBlip = 0;
	this->target = 0;
	this->isSetPrepared = false;
	this->guardsGroup = PED::CREATE_GROUP(0);
	this->enemiesAlertnessStatus = AlertnessStatus::Idle;
	this->willBountyHuntersSpawn = false;
	
	this->deliveryExecutor = NULL;
	this->bountyHunters = NULL;

	this->spawnDistance = DataFiles::GeneralMeta->getInt("bounty_mission.spawn_distance");
	this->blipRadius = DataFiles::GeneralMeta->getInt("bounty_mission.blip_radius");
	this->targetDetectionDistance = DataFiles::GeneralMeta->getInt("bounty_mission.target_detection_distance");
	this->targetDetectionLos = DataFiles::GeneralMeta->getInt("bounty_mission.target_detection_los");
	this->withMusic = true;
	this->targetAI = NULL;

	setObjective(BountyMissionObjective::GoToArea);

	if (status == BountyMissionStatus::Blocked)
	{
		block();
	}
	else
	{
		setStatus(status);
	}

	wasTargetConfronted = false;
	isTargetConfrontable = missionData->templateId == BountyMissionTemplateTypes::SoleCriminal || missionData->templateId == BountyMissionTemplateTypes::GraveRobber;

	this->confrontPrompt = new Prompt(
		DataFiles::Lang->get("prompts.confront"),
		joaat("INPUT_CONTEXT_X")
	);

	this->confrontPrompt->hide();
}

BountyMissionData* BaseBountyMissionExecutor::getMissionData()
{
	return missionData;
}

BountyMissionTemplateData* BaseBountyMissionExecutor::getMissionTemplateData()
{
	return BountyMissionTemplates::get(getMissionData()->templateId);
}

MapArea* BaseBountyMissionExecutor::getArea()
{
	return area;
}

BountyMissionStatus BaseBountyMissionExecutor::getStatus()
{
	return status;
}

BountyMissionObjective BaseBountyMissionExecutor::getObjective()
{
	return objective;
}

bool BaseBountyMissionExecutor::isTerminated()
{
	return getStatus() == BountyMissionStatus::Done || getStatus() == BountyMissionStatus::Failed;
}

bool BaseBountyMissionExecutor::isTargetAlive()
{
	return target && ENTITY::DOES_ENTITY_EXIST(target) && !ENTITY::IS_ENTITY_DEAD(target);
}

void BaseBountyMissionExecutor::block(bool withBlip)
{
	deleteBlipSafe(&areaBlip);
	
	if (withBlip)
	{
		createAreaBlip(0);
		MAP::BLIP_ADD_MODIFIER(areaBlip, 0x2B30E11F);
	}

	setStatus(BountyMissionStatus::Blocked);
}

void BaseBountyMissionExecutor::resume()
{
	deleteBlipSafe(&areaBlip);
	setObjective(BountyMissionObjective::GoToArea);
}

void BaseBountyMissionExecutor::update()
{
	if (isTerminated() || getStatus() == BountyMissionStatus::Blocked)
	{
		return;
	}

	if (PAD::IS_CONTROL_JUST_RELEASED(0, MISC::GET_HASH_KEY("INPUT_HUD_SPECIAL")) || 
		(deliveryExecutor && deliveryExecutor->hasObjectiveJustChanged()))
	{
		showObjectiveText();
	}

	if (target && ENTITY::DOES_ENTITY_EXIST(target) && ENTITY::IS_ENTITY_DEAD(target) && getMissionData()->requiredCondition == BountyTargetCondition::Alive)
	{
		showSubtitle(DataFiles::Lang->get("bounty_mission.fails.target_died"));
		fail("target was wanted alive");
	}

	bool wasTargetCaptured = deliveryExecutor && deliveryExecutor->getObjective() == BountyTargetDeliveryObjective::DeliverTarget;

	tick();

	bool isTargetCaptured = deliveryExecutor && deliveryExecutor->getObjective() == BountyTargetDeliveryObjective::DeliverTarget;

	if (allEnemies.size())
	{
		tickEnemiesAI();
	}

	if (willBountyHuntersSpawn && bountyHunters)
	{
		if (!bountyHunters->didSpawn() && bountyHunters->getSecondsToSpawnTime() <= 0 && getMapArea(player) != area->getAreaId())
		{
			Vector3 spawnCoords = getBountyHuntersSpawnCoords();
			bountyHunters->setSpawnCoords(spawnCoords);
			bountyHunters->spawn();

			if (bountyHunters->didSpawn())
			{
				bountyHunters->attack(player);
				setObjective(BountyMissionObjective::LosePursuit);
			}
		}
	}


	if (!wasTargetCaptured && isTargetCaptured)
	{
		uponTargetCapture();
	}
}

void BaseBountyMissionExecutor::cleanup()
{
	deleteBlipSafe(&areaBlip);
	deleteBlipSafe(&targetBlip);
	
	for each (Entity entity in missionEntities)
	{
		ENTITY::SET_ENTITY_AS_NO_LONGER_NEEDED(&entity);
	}

	if (deliveryExecutor)
	{
		deliveryExecutor->cleanup();
	}

	if (bountyHunters)
	{
		bountyHunters->cleanup();
	}

	if (confrontPrompt)
	{
		confrontPrompt->hide();
		delete confrontPrompt;
		confrontPrompt = NULL;
	}
	
	MISC::SET_MISSION_FLAG(false);

	AUDIO::TRIGGER_MUSIC_EVENT("RBT23_BANDITO_MINE_STOP");
	AUDIO::TRIGGER_MUSIC_EVENT("OBT3_ABANDON");
}

void BaseBountyMissionExecutor::fail(const char* reason)
{
	setStatus(BountyMissionStatus::Failed);

	string logMsg("BaseBountyMissionExecutor: mission failed, id: ");
	logMsg.append(to_string(getMissionData()->id));

	if (reason)
	{
		logMsg.append(" reason: ").append(reason);
	}

	log(logMsg);
}

int BaseBountyMissionExecutor::calculateReward()
{
	int base = getMissionData()->getScaledReward();

	if (isTargetAlive())
	{
		return base;
	}
	else
	{
		return roundToNearestMultipleOf(base * DataFiles::GeneralMeta->getFloat("bounty_target.delivery.dead_target_price"), 5);
	}
}

void BaseBountyMissionExecutor::setAreaBlipLabel(const char* label)
{
	if (areaBlip)
	{
		setBlipLabel(areaBlip, label);
	}
}

Vector3 BaseBountyMissionExecutor::getBountyHuntersSpawnCoords()
{
	Position bountyHuntersSpawnPos = getClosestVehicleNode(playerPos());
	if (!bountyHuntersSpawnPos.second)
	{
		return toVector3(0, 0, 0);
	}

	Vector3 result = calculateLocationAlongRoad(
		bountyHuntersSpawnPos.first, 
		rndInt(
			DataFiles::GeneralMeta->getInt("bounty_mission.bounty_hunters.min_spawn_distance"), 
			DataFiles::GeneralMeta->getInt("bounty_mission.bounty_hunters.max_spawn_distance") + 1),
		rndInt(0, 2) > 0
	);

	return result;
}

void BaseBountyMissionExecutor::createAreaBlip(int radius)
{
	areaBlip = createBlip(getMissionData()->startLocation, radius, 0xB04092F8, -907204276);
	
	setBlipLabel(areaBlip, string(DataFiles::Lang->get("bounty_mission.blips.bounty_mission"))
		.append(": ")
		.append(DataFiles::Lang->get(getMissionData()->requiredCondition == BountyTargetCondition::Alive ?  "bounty_mission.alive" : "bounty_mission.deadoralive"))
		.append(", $")
		.append(to_string(getMissionData()->getScaledReward())
	).c_str());
}

int BaseBountyMissionExecutor::getGuardPedModel()
{
	BountyMissionTemplateData* templateData = getMissionTemplateData();
	
	if (!templateData->guardPedModels->size())
	{
		return getMissionData()->targetsData.at(0)->pedModel;
	}

	vector<const char*>* guardModels = getMissionTemplateData()->guardPedModels;
	return joaat(guardModels->at(rndInt(0, guardModels->size())));
}

void BaseBountyMissionExecutor::showObjectiveText()
{
	if (getObjective() == BountyMissionObjective::DeliverTarget && deliveryExecutor)
	{
		if (deliveryExecutor->getObjective() == BountyTargetDeliveryObjective::CaptureTarget)
		{
			if (getMissionData()->requiredCondition == BountyTargetCondition::Alive)
			{
				showSubtitle(
					DataFiles::Lang->get("bounty_mission.objective.capture_alive")
				);
			}
			else
			{
				showSubtitle(
					DataFiles::Lang->get("bounty_mission.objective.capture_deadoralive")
				);
			}
		}
		else
		{
			deliveryExecutor->showObjectiveText();
		}
	}
	else
	{
		const char* text = DataFiles::Lang->get(
			string("bounty_mission.objectives.")
			.append(to_string((int)getObjective())).c_str()
		);

		if (strlen(text))
		{
			showSubtitle(text);
		}
	}
}

Ped BaseBountyMissionExecutor::getTarget()
{
	return target;
}

void BaseBountyMissionExecutor::decorateTarget()
{
	PED::SET_PED_AS_GROUP_LEADER(target, guardsGroup, 1);
	PED::SET_PED_CONFIG_FLAG(target, 4, 1); // PCF_HasBounty
	PED::SET_PED_CONFIG_FLAG(target, 6, 1); // PCF_DontInfluenceWantedLevel
	PED::SET_PED_CONFIG_FLAG(target, 278, 0); // PCF_ClearRadarBlipOnDeath
	
	DECORATOR::DECOR_SET_INT(target, "honor_override", 0);
	DECORATOR::DECOR_SET_INT(target, "SH_BOUNTIES_isBountyTarget", 1);

	PED::_SET_PED_PROMPT_NAME(target, getMissionData()->targetsData.at(0)->getName());
	confrontPrompt->setTargetEntity(target);

	float fleeChance = getMissionTemplateData()->targetFleeChance;
	float fleeScore = MISC::GET_RANDOM_FLOAT_IN_RANGE(0, 1);
	bool willFlee = fleeChance >= fleeScore;

	targetAI = new GuardAI(target);
	targetAI->setShouldFleeFromCombat(willFlee);
	targetAI->setShouldAutomaticallyCreateBlip(false);

	if (willFlee)
	{
		log("BaseBountyMissionExecutor: target will flee when threatned");
	}

	allEnemies.insert(targetAI);
}

Entity BaseBountyMissionExecutor::addMissionEntity(Entity entity)
{
	missionEntities.insert(entity);
	return entity;
}

Ped BaseBountyMissionExecutor::addGuard(Ped ped, bool addToGroup)
{
	PED::SET_PED_CONFIG_FLAG(ped, 6, 1);
	DECORATOR::DECOR_SET_INT(ped, "honor_override", 0);

	if (addToGroup)
	{
		PED::SET_PED_AS_GROUP_MEMBER(ped, guardsGroup);
	}

	GuardAI* guard = new GuardAI(ped);
	guard->setShouldAutomaticallyCreateBlip(false);

	guards.insert(guard);
	allEnemies.insert(guard);
	addMissionEntity(ped);

	return ped;
}

void BaseBountyMissionExecutor::setStatus(BountyMissionStatus status)
{
	log(string("BaseBountyMissionExecutor: status change in mission id ")
		.append(to_string(getMissionData()->id))
		.append(": ")
		.append(to_string((int)this->status))
		.append(" -> ")
		.append(to_string((int)status))
	);

	if (this->status != status && status == BountyMissionStatus::InProgress)
	{
		this->missionStartTimestamp = CLOCK::_GET_SECONDS_SINCE_BASE_YEAR();
		
		if (ScriptSettings::getBool("UseMissionFlag"))
		{
			MISC::SET_MISSION_FLAG(true);
		}
	}

	this->status = status;
}

void BaseBountyMissionExecutor::setObjective(BountyMissionObjective objective)
{
	log(string("BaseBountyMissionExecutor: objective change in mission id ")
		.append(to_string(getMissionData()->id))
		.append(": ")
		.append(to_string((int)this->objective))
		.append(" -> ")
		.append(to_string((int)objective))
	);
	this->objective = objective;

	if (status != BountyMissionStatus::InProgress && (
		objective == BountyMissionObjective::LocateTarget || 
		objective == BountyMissionObjective::DeliverTarget || 
		objective == BountyMissionObjective::LosePursuit)
	)
	{
		setStatus(BountyMissionStatus::InProgress);
	}
	else if (status != BountyMissionStatus::PosterCollected && objective == BountyMissionObjective::GoToArea)
	{
		setStatus(BountyMissionStatus::PosterCollected);
	}
	else if (status != BountyMissionStatus::Done && objective == BountyMissionObjective::Completed)
	{
		setStatus(BountyMissionStatus::Done);
	}

	showObjectiveText();
}

void BaseBountyMissionExecutor::tick()
{
	switch (getObjective())
	{
		case BountyMissionObjective::GoToArea:
		{
			if (!areaBlip)
			{
				createAreaBlip(blipRadius);
			}

			float distanceToStartLocation = distance(getMissionData()->startLocation, player);
			if ((!isSetPrepared || !target) && distanceToStartLocation <= spawnDistance)
			{
				log(string("BaseBountyMissionExecutor: preparing mission set. mission id: ")
					.append(to_string(getMissionData()->id))
					.append(", mission template: ")
					.append(to_string((int)getMissionData()->templateId))
					.append(", mission coords: ")
					.append(to_string(getMissionData()->startLocation))
				);
				prepareSet();
				target = spawnTarget();
				
				decorateTarget();
				addMissionEntity(target);
				isSetPrepared = true;
			}

			if (isSetPrepared && target && PLAYER::IS_PLAYER_FREE_AIMING_AT_ENTITY(PLAYER::PLAYER_ID(), target))
			{
				uponTargetDetection();
			}
			else if (distanceToStartLocation <= blipRadius)
			{
				uponMissionAreaArrival();
			}

			break;
		}
		case BountyMissionObjective::LocateTarget:
		{
			if (distance(getMissionData()->startLocation, player) >= DataFiles::GeneralMeta->getInt("bounty_mission.area_abandon_distance"))
			{
				uponMissionAreaAbandoned();
			}
			else if (distance(player, target) <= targetDetectionDistance ||
				PLAYER::IS_PLAYER_FREE_AIMING_AT_ENTITY(PLAYER::PLAYER_ID(), target) ||
				hasPedClearLosInFront(player, target, targetDetectionLos))
			{
				uponTargetDetection();
			}
			break;
		}
		case BountyMissionObjective::DeliverTarget:
		{
			float distanceToTarget = distance(target, player);
			if (distanceToTarget >= DataFiles::GeneralMeta->getInt("bounty_mission.target_abandon_distance"))
			{
				if (PED::IS_PED_FLEEING(getTarget()))
				{
					fail("target escaped");
					showSubtitle(DataFiles::Lang->get("bounty_mission.fails.target_lost"));
				}
				else
				{
					fail("mission abandoned");
					showSubtitle(DataFiles::Lang->get("bounty_mission.fails.target_abandoned"));
				}
			}
			else if (PLAYER::IS_PLAYER_DEAD(PLAYER::PLAYER_ID()))
			{
				fail("player died");
			}
			else
			{
				if (!deliveryExecutor)
				{
					deliveryExecutor = new BountyTargetDeliveryExecutor(target, area, getMissionData()->getScaledReward(), false);
				}

				if (deliveryExecutor->getObjective() != BountyTargetDeliveryObjective::Completed)
				{
					deliveryExecutor->setReward(calculateReward());
					deliveryExecutor->update();

					if (deliveryExecutor->getObjective() == BountyTargetDeliveryObjective::CaptureTarget && isTargetConfrontable)
					{
						confrontPrompt->show();
						if (confrontPrompt->isActivatedByPlayer())
						{
							playAmbientSpeech(player, "CALLOUT_AT_MALE_ARMED");
							confrontPrompt->setIsEnabled(false);
							wasTargetConfronted = true;
							WAIT(1000);
							uponTargetConfrontation();
						}
					}
					else
					{
						confrontPrompt->hide();

						if (deliveryExecutor->getObjective() == BountyTargetDeliveryObjective::ImprisonTarget && ScriptSettings::getBool("AllowMissionsMusic"))
						{
							AUDIO::TRIGGER_MUSIC_EVENT("OBT3_ABANDON");
						}
					}
				}
				else
				{
					setObjective(BountyMissionObjective::Completed);
				}
			}
			break;
		}
		case BountyMissionObjective::LosePursuit:
		{
			if (PLAYER::IS_PLAYER_DEAD(PLAYER::PLAYER_ID()))
			{
				fail("player died");
			}
			else if (!bountyHunters || bountyHunters->countBountyHuntersInRange(playerPos(), DataFiles::GeneralMeta->getInt("bounty_mission.bounty_hunters.despawn_distance")) == 0
			)
			{
				uponPursuitLost();
			}

			break;
		}
	}
}

void BaseBountyMissionExecutor::showEnemyBlips()
{
	if (!targetBlip)
	{
		targetBlip = createBlip(target, 0x38CDE89D /*BLIP_TYPE_BOUNTY_TARGET*/, 0x5846C31D /*BLIP_SPRITE_BOUNTY_TARGET*/);
		setBlipLabel(targetBlip, getMissionData()->targetsData.at(0)->getName());
	}

	for each (GuardAI* guard in guards)
	{
		guard->showBlip();
	}
}

void BaseBountyMissionExecutor::tickEnemiesAI()
{
	if (enemiesAlertnessStatus == AlertnessStatus::Combat)
	{
		return;
	}

	for each (GuardAI* enemy in allEnemies)
	{
		if (!enemy->isAlive())
		{
			continue;
		}

		enemy->update();

		if (enemy->getMode() == GuardAlertnessModes::Combat && enemy->getTimeSinceLastModeChange() >= 45)
		{
			combatPlayer();
			return;
		}
	}
}

void BaseBountyMissionExecutor::uponMissionAreaArrival()
{
	deleteBlipSafe(&areaBlip);
	setObjective(BountyMissionObjective::LocateTarget);
}

void BaseBountyMissionExecutor::uponMissionAreaAbandoned()
{
	setObjective(BountyMissionObjective::GoToArea);
	areaBlip = createBlip(getMissionData()->startLocation, blipRadius, 0xB04092F8, -907204276);
}

void BaseBountyMissionExecutor::uponTargetDetection()
{
	log("BaseBountyMissionExecutor: target found");

	if (areaBlip)
	{
		deleteBlipSafe(&areaBlip);
	}

	if (PLAYER::IS_PLAYER_FREE_AIMING_AT_ENTITY(PLAYER::PLAYER_ID(), target))
	{
		playAmbientSpeech(player, "HUNTING_NEAR");
	}

	showEnemyBlips();
	
	if (isTargetConfrontable)
	{
		confrontPrompt->show();
	}

	if (ScriptSettings::getBool("EnableRivalBountyHunters") && getMissionData()->requiredCondition == BountyTargetCondition::DeadOrAlive)
	{
		float bountyHuntersSpawnScore = MISC::GET_RANDOM_FLOAT_IN_RANGE(0, 1);
		float bountyHuntersSpawnChance = this->getMissionData()->bountyHuntersSpawnChance * ScriptSettings::getFloat("BountyHuntersSpawnChanceFactor");
		willBountyHuntersSpawn = bountyHuntersSpawnChance >= bountyHuntersSpawnScore;
	
		if (willBountyHuntersSpawn) 
		{
			bountyHunters = new BountyHuntersGroup(
				DataFiles::GeneralMeta->getInt("bounty_mission.bounty_hunters.amount"), 
				toVector3(0,0,0), 
				CLOCK::_GET_SECONDS_SINCE_BASE_YEAR() + rndInt(
					DataFiles::GeneralMeta->getInt("bounty_mission.bounty_hunters.max_secs_to_spawn"), 
					DataFiles::GeneralMeta->getInt("bounty_mission.bounty_hunters.min_secs_to_spawn"))
			);
		}

		log(string("BaseBountyMissionExecutor: bounty hunters spawn score: ")
			.append(to_string(bountyHuntersSpawnScore))
			.append(" vs chance: ")
			.append(to_string(bountyHuntersSpawnChance))
			.append(", result: ")
			.append(!willBountyHuntersSpawn ? "bounty hunters won't spawn" : 
				string("bounty hunters will spawn in game secs: ")
				.append(to_string(bountyHunters->getSecondsToSpawnTime()))
			)
		);
	}
	else
	{
		log("BaseBountyMissionExecutor: rival bounty hunters preference disabled - bounty hunters won't spawn");
	}

	setObjective(BountyMissionObjective::DeliverTarget);
}

void BaseBountyMissionExecutor::uponTargetConfrontation()
{
	log("BaseBountyMissionExecutor: target confronted");
	combatPlayer();
}

void BaseBountyMissionExecutor::uponPursuitLost()
{
	if (bountyHunters)
	{
		bountyHunters->cleanup();
	}

	setObjective(BountyMissionObjective::DeliverTarget);
}

void BaseBountyMissionExecutor::combatPlayer()
{
	if (this->objective < BountyMissionObjective::DeliverTarget)
	{
		uponTargetDetection();
	}

	enemiesAlertnessStatus = AlertnessStatus::Combat;
	log("BaseBountyMissionExecutor: combat initiated");

	showEnemyBlips();

	for each (GuardAI* enemy in allEnemies)
	{
		if (enemy->ped() == target && enemy->getShouldFleeFromCombat())
		{
			uponTargetFlee();
		}
		
		if (enemy->getMode() != GuardAlertnessModes::Combat)
		{
			enemy->enterCombatMode(player);
		}
	}
}

void BaseBountyMissionExecutor::uponTargetCapture()
{
	log("BaseBountyMissionExecutor: target captured");

	AUDIO::TRIGGER_MUSIC_EVENT("RBT23_BANDITO_MINE_STOP");

	if (ScriptSettings::getBool("AllowMissionsMusic"))
	{
		AUDIO::TRIGGER_MUSIC_EVENT("OBT3_APPROACH"); 
	}
}

void BaseBountyMissionExecutor::uponTargetFlee()
{
	log("BaseBountyMissionExecutor: target is fleeing");

	if (ScriptSettings::getBool("AllowMissionsMusic"))
	{
		AUDIO::TRIGGER_MUSIC_EVENT("RBT23_BANDITO_MINE_START");
	}
}
