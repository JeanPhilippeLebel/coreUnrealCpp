// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "FirstPersonShootCPPGameMode.h"
#include "FirstPersonShootCPPHUD.h"
#include "FirstPersonShootCPPCharacter.h"
#include "FirstPersonShootCPPProjectile.h"

#include "UObject/ConstructorHelpers.h"

//coreDS Unreal
#include "coreDSSettingsClass.h"
#include "coreDSEngine.h"
#include "coreDS_BPCoordinateConversion.h"

AFirstPersonShootCPPGameMode::AFirstPersonShootCPPGameMode()
	: Super()
{
	// set default pawn class to our Blueprinted character
	static ConstructorHelpers::FClassFinder<APawn> PlayerPawnClassFinder(TEXT("/Game/FirstPersonCPP/Blueprints/FirstPersonCharacter"));
	DefaultPawnClass = PlayerPawnClassFinder.Class;

	// use our custom HUD class
	HUDClass = AFirstPersonShootCPPHUD::StaticClass();

	// coreDS Unreal
	// create our configuration settings. This could be done through the Editor but in case you want to 
	// manage the configure, you can do it here.
	UcoreDSSettings* lSettings = const_cast<UcoreDSSettings*>(GetDefault<UcoreDSSettings>());

	//this will overwrite the values - be careful if you are using the Editor
	lSettings->ReferenceLatitude = 46.8298531;
	lSettings->ReferenceLongitude = -71.2540283;
	lSettings->ReferenceAltitude = 5.0;

	//Object your simulator can accept 
	lSettings->SupportedOutputObjects.AddUnique("Gun.Location.x");
	lSettings->SupportedOutputObjects.AddUnique("Gun.Location.y");
	lSettings->SupportedOutputObjects.AddUnique("Gun.Location.z");
	lSettings->SupportedOutputObjects.AddUnique("Gun.Orientation.pitch");
	lSettings->SupportedOutputObjects.AddUnique("Gun.Orientation.yaw");
	lSettings->SupportedOutputObjects.AddUnique("Gun.Orientation.roll");
	lSettings->SupportedOutputObjects.AddUnique("Bullet.Location.x");
	lSettings->SupportedOutputObjects.AddUnique("Bullet.Location.y");
	lSettings->SupportedOutputObjects.AddUnique("Bullet.Location.z");

	//Message your simulator can accept
	lSettings->SupportedOutputMessages.AddUnique("ShotFired.Location.x");
	lSettings->SupportedOutputMessages.AddUnique("ShotFired.Location.y");
	lSettings->SupportedOutputMessages.AddUnique("ShotFired.Location.z");

	//Object your simulator can send 
	lSettings->SupportedInputObjects.AddUnique("Gun.Location.x");
	lSettings->SupportedInputObjects.AddUnique("Gun.Location.y");
	lSettings->SupportedInputObjects.AddUnique("Gun.Location.z");
	lSettings->SupportedInputObjects.AddUnique("Gun.Orientation.pitch");
	lSettings->SupportedInputObjects.AddUnique("Gun.Orientation.yaw");
	lSettings->SupportedInputObjects.AddUnique("Gun.Orientation.roll");
	lSettings->SupportedInputObjects.AddUnique("Bullet.Location.x");
	lSettings->SupportedInputObjects.AddUnique("Bullet.Location.y");
	lSettings->SupportedInputObjects.AddUnique("Bullet.Location.z");

	//Message your simulator can send
	lSettings->SupportedInputMessages.AddUnique("ShotFired.Location.x");
	lSettings->SupportedInputMessages.AddUnique("ShotFired.Location.y");
	lSettings->SupportedInputMessages.AddUnique("ShotFired.Location.z");

	lSettings->SaveConfig();

	// enable tick
	PrimaryActorTick.bCanEverTick = true;

}

void AFirstPersonShootCPPGameMode::BeginPlay()
{
	// Call the base class  
	Super::BeginPlay();

	Engine = GetGameInstance()->GetSubsystem<UcoreDSEngine>();

	//Add a callback to be aware when a coreDS based entities is being deleted by Unreal
	//GEngine->OnLevelActorDeleted().Add(&AFirstPersonShootCPPGameMode::objectDeletedFromLevel);
	//GEngine->OnLevelActorDeleted().Add(this, &AFirstPersonShootCPPGameMode::objectDeletedFromLevel);
	OnLevelActorDeletedHandle = GEngine->OnLevelActorDeleted().AddUObject(this, &AFirstPersonShootCPPGameMode::objectDeletedFromLevel);

	// coreDS Unreal
	Engine->connect();
	
	//create our delegates that will handle received informations from the distributed simulation backend

	//Error handler
	FErrorReceivedHandler lErrorHandler;
	lErrorHandler.BindUFunction(this, "printErrorDelegate");

	//New gun position received
	FObjectUpdateHandler lObjectUpdateHandlerForGuns;
	lObjectUpdateHandlerForGuns.BindUFunction(this, "gunUpdated");

	//New bullet position received
	FObjectUpdateHandler lObjectUpdateHandlerForBullets;
	lObjectUpdateHandlerForBullets.BindUFunction(this, "bulletUpdated");

	// An object has been removed
	FObjectRemovedHandler lObjectRemovedHandler;
	lObjectRemovedHandler.BindUFunction(this, "objectRemoved");
	
	// WeaponFire message received
	FMessageUpdateHandler lMessageUpdateHandlerWeaponFire;
	lMessageUpdateHandlerWeaponFire.BindUFunction(this, "shotFiredMessageReceived");


	//register the required callbacks to received information for HLA or DIS
	///Register a callback to report an error
	Engine->registerErrorReceivedHandler(lErrorHandler);

	///Register a callback to an update object event
	Engine->registerObjectUpdateHandler("Gun", lObjectUpdateHandlerForGuns);
	Engine->registerObjectUpdateHandler("Bullet", lObjectUpdateHandlerForBullets);

	///Register a callback to an object removed
	Engine->registerObjectRemovedHandler("Gun", lObjectRemovedHandler);
	Engine->registerObjectRemovedHandler("Bullet", lObjectRemovedHandler);

	///Register a callback to a message is received
	Engine->registerMessageUpdateHandler("ShotFired", lMessageUpdateHandlerWeaponFire);
}

void AFirstPersonShootCPPGameMode::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	// Call the base class  
	Super::EndPlay(EndPlayReason);

	// coreDS Unreal
	Engine->disconnect();
}

void AFirstPersonShootCPPGameMode::printErrorDelegate(FString Message, int Errorcode)
{
	GEngine->AddOnScreenDebugMessage(-1, 5.f, FColor::Red, *Message);
	UE_LOG(LogClass, Log, TEXT("coreDS: %s"), *Message);
}

// Values are received in a list of value-pair. Each pair consists of the value name, as defined in the mapping and the value as a string
// ObjectName is the unique object identifier
void  AFirstPersonShootCPPGameMode::gunUpdated(FCoreDSVariant Values, FString ObjectName)
{
	spawnActorBasedOntype(DefaultPawnClass, Values, ObjectName);
}

void  AFirstPersonShootCPPGameMode::bulletUpdated(FCoreDSVariant Values, FString ObjectName)
{
	spawnActorBasedOntype(Cast<AFirstPersonShootCPPCharacter>(GetWorld()->GetFirstPlayerController()->GetPawn())->ProjectileClass, Values, ObjectName);
}

void AFirstPersonShootCPPGameMode::objectDeletedFromLevel(AActor* DeletedActor)
{
	FScopeLock lock(&mDiscoveredObjectMutex);

	if (mDiscoveredObjectRev.Contains(DeletedActor))
	{
		mDiscoveredObject.Remove(mDiscoveredObjectRev[DeletedActor]);
		mDiscoveredObjectRev.Remove(DeletedActor);
	}
}

void  AFirstPersonShootCPPGameMode::spawnActorBasedOntype(TSubclassOf<AActor> ActorType, FCoreDSVariant Values, FString ObjectName)
{
	//check if we have all valid values
	if (!Values.exists("Location.x") || !Values.exists("Location.y") || !Values.exists("Location.z"))
	{
		printErrorDelegate("Missing values when receiving data - please check your mapping", 0);
		return;
	}

	FRotator lRot(0,0,0);
	if (Values.exists("Orientation.pitch") && Values.exists("Orientation.yaw") && Values.exists("Orientation.roll"))
	{
		lRot = FRotator(Values["Orientation.pitch"].toDouble(), Values["Orientation.yaw"].toDouble(), Values["Orientation.roll"].toDouble());
	}

	//UE_LOG(LogClass, Log, TEXT("coreDS: Received position %g, %g, %g"), Values["Location.x"].toFloat(), Values["Location.y"].toFloat(), Values["Location.z"].toFloat());

	float xEnu = 0, yEnu = 0, zEnu = 0;
	float pitch = 0, roll = 0, yaw = 0;
	UcoreDSSettings* lSettings = const_cast<UcoreDSSettings*>(GetDefault<UcoreDSSettings>());
	UCoreDSCoordinateConversion::EcefToEnu_float(Values["Location.x"].toFloat(), Values["Location.y"].toFloat(), Values["Location.z"].toFloat(), 
		xEnu, yEnu, zEnu, 
		roll, pitch, yaw, 
		lRot.Pitch, lRot.Roll, lRot.Yaw, 
		lSettings->ReferenceLatitude, lSettings->ReferenceLongitude, lSettings->ReferenceAltitude);

	lRot = FRotator(pitch, yaw, roll);
	FVector lNewLocation(xEnu, yEnu, zEnu);

	//UE_LOG(LogClass, Log, TEXT("coreDS: Converted location %g, %g, %g"), xEnu, yEnu, zEnu);


	FTransform lTransform(lRot, lNewLocation);

	AActor *lActor = NULL;
	
	FScopeLock lock(&mDiscoveredObjectMutex);

	if (!mDiscoveredObject.Contains(ObjectName))
	{
		//not yet discovered
		lActor = Cast<AActor>(UGameplayStatics::BeginDeferredActorSpawnFromClass(GetWorld(), ActorType, lTransform));
		if (lActor != nullptr)
		{
			//we need to add the tag before the object is created to prevent a ping-pong effect
			lActor->Tags.Add("coreDSCreated");

			if (IsValid(lActor))
			{
				lActor->SetActorEnableCollision(false);
				lActor->DisableComponentsSimulatePhysics();

				//check if the object was already discovered
				if (!mDiscoveredObject.Contains(ObjectName))
				{
					//spawn the object
					mDiscoveredObject.Emplace(ObjectName, lActor);
					mDiscoveredObjectRev.Emplace(lActor, ObjectName);
				}
			}

			UGameplayStatics::FinishSpawningActor(lActor, lTransform);

			if (IsValid(lActor))
			{
				FString lFinaleName = ObjectName;
				lFinaleName.Append(" (coreDS)");
				lActor->SetActorLabel(lFinaleName);
			}
			else
			{
				printErrorDelegate("Could not create actor", 0);
			}
		}
	}
	else
	{
		//update the position
		lActor = mDiscoveredObject[ObjectName];

		if (IsValid(lActor))
		{
			//make sure the object is within the scene, otherwise it will get destroyed
			lActor->SetActorLocationAndRotation(lNewLocation, lRot, false, nullptr, ETeleportType::ResetPhysics);
		}
	}
}

//Play a sound when the ShotFired message is received
void  AFirstPersonShootCPPGameMode::shotFiredMessageReceived(FCoreDSVariant Values)
{
	//find the Character instance so we can play the fire sound
	AFirstPersonShootCPPCharacter* myCharacter = Cast<AFirstPersonShootCPPCharacter>(GetWorld()->GetFirstPlayerController()->GetPawn());

	// try and play the sound if specified
	if (myCharacter != NULL && myCharacter->FireSound != NULL)
	{
		UGameplayStatics::PlaySoundAtLocation(this, myCharacter->FireSound, myCharacter->GetActorLocation());
	}
}

void  AFirstPersonShootCPPGameMode::objectRemoved(FString ObjectName)
{
	FScopeLock lock(&mDiscoveredObjectMutex);

	if (mDiscoveredObject.Contains(ObjectName))
	{
		AActor *lActor = mDiscoveredObject[ObjectName];

		//remove the object from the scene
		if (lActor && lActor->IsValidLowLevel() || !lActor->IsActorBeingDestroyed() || !lActor->IsPendingKill() || !lActor->IsPendingKillOrUnreachable()
			|| !lActor->IsPendingKillPending())
		{
			lActor->MarkPendingKill();
		}

		mDiscoveredObject.Remove(ObjectName);
		mDiscoveredObjectRev.Remove(lActor);
	}
}

void  AFirstPersonShootCPPGameMode::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	Engine->step();
}