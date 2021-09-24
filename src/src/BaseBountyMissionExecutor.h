#pragma once

enum class AlertnessStatus : int
{
	Idle,
	Search,
	Combat
};

class BaseBountyMissionExecutor
{
private:
	BountyMissionData* missionData; 
	MapArea* area;
	BountyMissionStatus status;
	BountyMissionObjective objective;
	
	int guardsGroup;
	Ped target;
	bool isSetPrepared;
	Blip areaBlip;
	Blip targetBlip;
	int missionStartTimestamp;
	bool willBountyHuntersSpawn;
	bool wasTargetConfronted;

	BountyTargetDeliveryExecutor* deliveryExecutor;
	BountyHuntersGroup* bountyHunters;
	Prompt* confrontPrompt;

protected:
	float spawnDistance;
	float blipRadius;
	float targetDetectionDistance;
	float targetDetectionLos;
	bool withMusic;
	bool isTargetConfrontable;

	AlertnessStatus enemiesAlertnessStatus;
	set<Entity> missionEntities;
	set<GuardAI*> guards;
	set<GuardAI*> allEnemies;
	GuardAI* targetAI;
		
public:
	BaseBountyMissionExecutor(BountyMissionData* missionData, MapArea* area, BountyMissionStatus status = BountyMissionStatus::Pending);
	BountyMissionData* getMissionData();
	BountyMissionTemplateData* getMissionTemplateData();
	MapArea* getArea();
	BountyMissionStatus getStatus();
	BountyMissionObjective getObjective();
	bool isTerminated();
	bool isTargetAlive();

	virtual void block(bool withBlip = true);
	virtual void resume();
	void update();
	virtual void cleanup();
	virtual void fail(const char* reason = NULL);
	int calculateReward();
	void setAreaBlipLabel(const char* label);

protected:
	Vector3 getBountyHuntersSpawnCoords();
	void createAreaBlip(int radius);
	int getGuardPedModel();
	void showObjectiveText();
	Ped getTarget();
	virtual void prepareSet() = 0;
	virtual Ped spawnTarget() = 0;
	virtual void decorateTarget();
	Entity addMissionEntity(Entity entity);
	virtual Ped addGuard(Ped ped, bool addToGroup = true);
	virtual void setStatus(BountyMissionStatus status);
	virtual void setObjective(BountyMissionObjective objective);
	virtual void tick();
	virtual void tickEnemiesAI();
	virtual void showEnemyBlips();

	virtual void uponMissionAreaArrival();
	virtual void uponMissionAreaAbandoned();
	virtual void uponTargetDetection();
	virtual void uponTargetConfrontation();
	virtual void uponPursuitLost();
	virtual void combatPlayer();
	virtual void uponTargetCapture();
	virtual void uponTargetFlee();
};

#include "CampBountyMissionExecutor.h"
#include "ShackBountyMissionExecutor.h"
#include "AmbushBountyMissionExecutor.h"
#include "GraveRobberBountyMissionExecutor.h"