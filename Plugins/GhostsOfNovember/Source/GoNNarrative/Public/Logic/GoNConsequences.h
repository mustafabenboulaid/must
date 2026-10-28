// Ghosts of November — persistent consequence state (pure C++).
// Suspicion, trust, network health, people saved/lost (stats) and known information (flags).
// Carries over between missions; never alters documented history (see ReservedHistoryPrefix).
#pragma once

#include "Logic/GoNJson.h"
#include "Logic/GoNNarrativeTypes.h"

#include <map>
#include <set>
#include <string>
#include <vector>

namespace GoN::Narrative
{
	class FConsequenceState
	{
	public:
		explicit FConsequenceState(const FConsequenceSchema* InSchema = nullptr) : Schema(InSchema) {}

		void SetSchema(const FConsequenceSchema* InSchema) { Schema = InSchema; }
		const FConsequenceSchema* GetSchema() const { return Schema; }

		void Reset();

		// Unset stats read as their schema default (0 when undeclared).
		double GetStat(const std::string& Key) const;
		bool HasFlag(const std::string& Key) const { return Flags.count(Key) > 0; }

		// Returns false (and explains why) for undeclared keys or reserved history keys.
		bool Apply(const FEffect& Effect, std::string* OutError = nullptr);
		bool ApplyAll(const std::vector<FEffect>& Effects, std::vector<std::string>* OutErrors = nullptr);

		bool Evaluate(const FCondition& Condition) const;
		bool EvaluateAll(const std::vector<FCondition>& Conditions) const;

		const std::map<std::string, double>& GetStats() const { return Stats; }
		const std::set<std::string>& GetFlags() const { return Flags; }

		Json::FValue ToJson() const;
		// Values are re-clamped against the current schema; unknown keys are dropped and reported.
		bool FromJson(const Json::FValue& Value, std::vector<std::string>* OutWarnings = nullptr);

		std::string DebugString() const;

	private:
		const FConsequenceSchema* Schema = nullptr;
		std::map<std::string, double> Stats;
		std::set<std::string> Flags;
	};

	const char* EffectOpName(EEffectOp Op);
	const char* CompareName(ECompare Compare);
	bool ParseEffectOp(const std::string& Text, EEffectOp& Out);
	bool ParseCompare(const std::string& Text, ECompare& Out);

	// Static check of an effect against the schema (used by the data validator).
	bool IsEffectAllowed(const FConsequenceSchema& Schema, const FEffect& Effect, std::string* OutError);
	bool IsConditionKnown(const FConsequenceSchema& Schema, const FCondition& Condition, std::string* OutError);
} // namespace GoN::Narrative
