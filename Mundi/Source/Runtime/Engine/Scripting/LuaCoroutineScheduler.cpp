#include "pch.h"
#include "LuaCoroutineScheduler.h"

void FLuaCoroutineScheduler::ShutdownBeforeLuaClose()
{
	for (auto& Task : Tasks)
	{
		if (Task.Co.valid())
		{
			Task.Co.abandon(); // Lua쪽 Coroutine 무력화 필수
		}
	}
	Tasks.Empty(); 
}

FLuaCoroHandle FLuaCoroutineScheduler::Register(sol::coroutine&& Co, void* Owner)
{
	FCoroTask Task;
	Task.Co     = std::move(Co);
	Task.Owner  = Owner;
	Task.Id     = ++NextId;
	Tasks.push_back(std::move(Task));

	return FLuaCoroHandle{ Task.Id };
}

void FLuaCoroutineScheduler::Tick(double DeltaTime)
{
	const double Clamped = std::min(DeltaTime, MaxDeltaClamp);
	NowSeconds += Clamped;

	Process(NowSeconds);
}
void FLuaCoroutineScheduler::Process(double Now)
{
	for (auto& task : Tasks)
	{
		if (task.Finished)
			continue;

		// 조건 충족 여부
		switch (task.WaitType)
		{
		case EWaitType::Time:
			// UE_LOG("Now=%.3f Wake=%.3f", TotalTime, task.WakeTime);
			if (task.WakeTime > Now) continue;
			break;
		case EWaitType::Predicate:
			// 람다 함수가 있지만, 조건이 달성 안 됐을 때
			if (task.Predicate && !task.Predicate()) continue; 
			break;
			// case EWaitType::Event: // Event "Trigger"는 Tick 함수 내에서 조건 확인하지 않음
		}

		// 조건 충족 시 resume 실행
		sol::protected_function_result Result = task.Co();
		if (!Result.valid())
		{
			sol::error Err = Result;
			UE_LOG("[Lua] Coroutine error: %s\n", Err.what());
			task.Finished = true;
			continue;
		}

		// 이후 yield가 다시 올 경우, 다음 조건 실행 = 재세팅
		if (Result.status() == sol::call_status::yielded)
		{
			std::string Tag = Result.get<FString>(0); // 해당 Co의 첫번째 string 매개변수
			if (Tag == "wait_time")
			{
				double Sec = Result.get<double>(1);
				task.WaitType = EWaitType::Time;
				task.WakeTime = Now + Sec;
			}
			else if (Tag == "wait_predicate")
			{
				sol::function Condition = Result.get<sol::function>(1); 
				task.WaitType = EWaitType::Predicate;
				task.Predicate = [Condition]()
				{
					sol::protected_function_result Result = Condition();
					if (!Result.valid()) return false; 
					return Result.get<bool>();
				};
			}
			else if (Tag == "wait_event")
			{
				task.WaitType = EWaitType::Event;
				task.EventName = Result.get<FString>(1);
			}
			else
			{
				task.WaitType = EWaitType::None;
			}
		}
		else
		{
			task.Finished = true;
		}
	}
}


void FLuaCoroutineScheduler::AddCoroutine(sol::coroutine&& Co)
{
	Tasks.push_back({std::move(Co)});
}

void FLuaCoroutineScheduler::TriggerEvent(const FString& EventName)
{
	for (auto& task : Tasks)
	{
		if (task.Finished) continue;
		if (task.WaitType != EWaitType::Event) continue;
		if (task.EventName == EventName)
		{
			task.WaitType = EWaitType::None;
			task.Co(); // resume
		}
	}
}

void FLuaCoroutineScheduler::CancelByOwner(void* Owner)
{
	for (auto& Task : Tasks) {
		if (Task.Owner == Owner && !Task.Finished) {
			Task.Finished = true;
			Task.Co = sol::coroutine(); // 참조 해제
		}
	}
}
