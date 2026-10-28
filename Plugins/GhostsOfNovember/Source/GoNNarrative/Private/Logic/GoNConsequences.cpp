#include "Logic/GoNConsequences.h"

#include <cmath>

namespace GoN::Narrative
{
	namespace ConsequencesDetail
	{
		bool StartsWith(const std::string& Text, const char* Prefix)
		{
			return Text.rfind(Prefix, 0) == 0;
		}

		double Clamp(double Value, double Lo, double Hi)
		{
			return Value < Lo ? Lo : (Value > Hi ? Hi : Value);
		}

		bool IsFlagOp(EEffectOp Op)
		{
			return Op == EEffectOp::SetFlag || Op == EEffectOp::ClearFlag;
		}

		bool IsFlagCompare(ECompare Compare)
		{
			return Compare == ECompare::FlagSet || Compare == ECompare::FlagUnset;
		}
	} // namespace ConsequencesDetail

	const char* EffectOpName(EEffectOp Op)
	{
		switch (Op)
		{
		case EEffectOp::Add: return "add";
		case EEffectOp::Set: return "set";
		case EEffectOp::SetFlag: return "setFlag";
		case EEffectOp::ClearFlag: return "clearFlag";
		}
		return "?";
	}

	const char* CompareName(ECompare Compare)
	{
		switch (Compare)
		{
		case ECompare::Equal: return "==";
		case ECompare::NotEqual: return "!=";
		case ECompare::Less: return "<";
		case ECompare::LessEqual: return "<=";
		case ECompare::Greater: return ">";
		case ECompare::GreaterEqual: return ">=";
		case ECompare::FlagSet: return "set";
		case ECompare::FlagUnset: return "unset";
		}
		return "?";
	}

	bool ParseEffectOp(const std::string& Text, EEffectOp& Out)
	{
		if (Text == "add") { Out = EEffectOp::Add; return true; }
		if (Text == "set") { Out = EEffectOp::Set; return true; }
		if (Text == "setFlag") { Out = EEffectOp::SetFlag; return true; }
		if (Text == "clearFlag") { Out = EEffectOp::ClearFlag; return true; }
		return false;
	}

	bool ParseCompare(const std::string& Text, ECompare& Out)
	{
		if (Text == "==") { Out = ECompare::Equal; return true; }
		if (Text == "!=") { Out = ECompare::NotEqual; return true; }
		if (Text == "<") { Out = ECompare::Less; return true; }
		if (Text == "<=") { Out = ECompare::LessEqual; return true; }
		if (Text == ">") { Out = ECompare::Greater; return true; }
		if (Text == ">=") { Out = ECompare::GreaterEqual; return true; }
		if (Text == "set") { Out = ECompare::FlagSet; return true; }
		if (Text == "unset") { Out = ECompare::FlagUnset; return true; }
		return false;
	}

	bool IsEffectAllowed(const FConsequenceSchema& Schema, const FEffect& Effect, std::string* OutError)
	{
		if (Effect.Key.empty())
		{
			if (OutError) { *OutError = "effect has no key"; }
			return false;
		}
		if (ConsequencesDetail::StartsWith(Effect.Key, ReservedHistoryPrefix()))
		{
			if (OutError) { *OutError = "'" + Effect.Key + "' is documented history; gameplay cannot change it"; }
			return false;
		}
		if (ConsequencesDetail::IsFlagOp(Effect.Op))
		{
			if (!Schema.IsKnownFlag(Effect.Key))
			{
				if (OutError) { *OutError = "flag '" + Effect.Key + "' is not declared in consequences.json"; }
				return false;
			}
			return true;
		}
		if (Schema.FindStat(Effect.Key) == nullptr)
		{
			if (OutError) { *OutError = "stat '" + Effect.Key + "' is not declared in consequences.json"; }
			return false;
		}
		if (!std::isfinite(Effect.Value))
		{
			if (OutError) { *OutError = "effect value for '" + Effect.Key + "' is not finite"; }
			return false;
		}
		return true;
	}

	bool IsConditionKnown(const FConsequenceSchema& Schema, const FCondition& Condition, std::string* OutError)
	{
		const bool bKnown = ConsequencesDetail::IsFlagCompare(Condition.Compare) ? Schema.IsKnownFlag(Condition.Key) : Schema.FindStat(Condition.Key) != nullptr;
		if (!bKnown && OutError)
		{
			*OutError = "condition reads undeclared key '" + Condition.Key + "'";
		}
		return bKnown;
	}

	void FConsequenceState::Reset()
	{
		Stats.clear();
		Flags.clear();
	}

	double FConsequenceState::GetStat(const std::string& Key) const
	{
		const auto It = Stats.find(Key);
		if (It != Stats.end())
		{
			return It->second;
		}
		const FStatDef* Def = Schema ? Schema->FindStat(Key) : nullptr;
		return Def ? Def->Default : 0.0;
	}

	bool FConsequenceState::Apply(const FEffect& Effect, std::string* OutError)
	{
		if (Schema == nullptr)
		{
			if (OutError) { *OutError = "consequence state has no schema"; }
			return false;
		}
		if (!IsEffectAllowed(*Schema, Effect, OutError))
		{
			return false;
		}
		switch (Effect.Op)
		{
		case EEffectOp::SetFlag:
			Flags.insert(Effect.Key);
			return true;
		case EEffectOp::ClearFlag:
			Flags.erase(Effect.Key);
			return true;
		case EEffectOp::Add:
		case EEffectOp::Set:
		{
			const FStatDef* Def = Schema->FindStat(Effect.Key);
			const double Base = Effect.Op == EEffectOp::Add ? GetStat(Effect.Key) : 0.0;
			Stats[Effect.Key] = ConsequencesDetail::Clamp(Base + Effect.Value, Def->Min, Def->Max);
			return true;
		}
		}
		return false;
	}

	bool FConsequenceState::ApplyAll(const std::vector<FEffect>& Effects, std::vector<std::string>* OutErrors)
	{
		bool bAllApplied = true;
		for (const FEffect& Effect : Effects)
		{
			std::string Error;
			if (!Apply(Effect, &Error))
			{
				bAllApplied = false;
				if (OutErrors)
				{
					OutErrors->push_back(Error);
				}
			}
		}
		return bAllApplied;
	}

	bool FConsequenceState::Evaluate(const FCondition& Condition) const
	{
		if (Condition.Compare == ECompare::FlagSet)
		{
			return HasFlag(Condition.Key);
		}
		if (Condition.Compare == ECompare::FlagUnset)
		{
			return !HasFlag(Condition.Key);
		}
		const double Value = GetStat(Condition.Key);
		constexpr double Epsilon = 1e-9;
		switch (Condition.Compare)
		{
		case ECompare::Equal: return std::fabs(Value - Condition.Value) < Epsilon;
		case ECompare::NotEqual: return std::fabs(Value - Condition.Value) >= Epsilon;
		case ECompare::Less: return Value < Condition.Value;
		case ECompare::LessEqual: return Value <= Condition.Value + Epsilon;
		case ECompare::Greater: return Value > Condition.Value;
		case ECompare::GreaterEqual: return Value + Epsilon >= Condition.Value;
		case ECompare::FlagSet:
		case ECompare::FlagUnset:
			break;
		}
		return false;
	}

	bool FConsequenceState::EvaluateAll(const std::vector<FCondition>& Conditions) const
	{
		for (const FCondition& Condition : Conditions)
		{
			if (!Evaluate(Condition))
			{
				return false;
			}
		}
		return true;
	}

	Json::FValue FConsequenceState::ToJson() const
	{
		Json::FValue Root = Json::FValue::MakeObject();
		Json::FValue StatsJson = Json::FValue::MakeObject();
		for (const auto& Pair : Stats)
		{
			StatsJson.Set(Pair.first, Json::FValue::MakeNumber(Pair.second));
		}
		Json::FValue FlagsJson = Json::FValue::MakeArray();
		for (const std::string& Flag : Flags)
		{
			FlagsJson.Push(Json::FValue::MakeString(Flag));
		}
		Root.Set("stats", std::move(StatsJson));
		Root.Set("flags", std::move(FlagsJson));
		return Root;
	}

	bool FConsequenceState::FromJson(const Json::FValue& Value, std::vector<std::string>* OutWarnings)
	{
		if (!Value.IsObject())
		{
			return false;
		}
		Reset();
		if (const Json::FValue* StatsJson = Value.Find("stats"))
		{
			for (const auto& Member : StatsJson->Members())
			{
				const FStatDef* Def = Schema ? Schema->FindStat(Member.first) : nullptr;
				if (Def == nullptr || !Member.second.IsNumber())
				{
					if (OutWarnings) { OutWarnings->push_back("dropped unknown stat '" + Member.first + "'"); }
					continue;
				}
				Stats[Member.first] = ConsequencesDetail::Clamp(Member.second.AsNumber(), Def->Min, Def->Max);
			}
		}
		if (const Json::FValue* FlagsJson = Value.Find("flags"))
		{
			for (const Json::FValue& Item : FlagsJson->Items())
			{
				if (!Item.IsString() || (Schema && !Schema->IsKnownFlag(Item.AsString())))
				{
					if (OutWarnings) { OutWarnings->push_back("dropped unknown flag '" + Item.AsString() + "'"); }
					continue;
				}
				Flags.insert(Item.AsString());
			}
		}
		return true;
	}

	std::string FConsequenceState::DebugString() const
	{
		std::string Out;
		for (const auto& Pair : Stats)
		{
			char Buffer[32];
			std::snprintf(Buffer, sizeof(Buffer), "%g", Pair.second);
			Out += Pair.first + "=" + Buffer + " ";
		}
		for (const std::string& Flag : Flags)
		{
			Out += "[" + Flag + "] ";
		}
		return Out.empty() ? std::string("(empty)") : Out;
	}
} // namespace GoN::Narrative
