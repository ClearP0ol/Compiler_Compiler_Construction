#ifndef SLRANALYSISTABLE_HPP
#define SLRANALYSISTABLE_HPP


#include <map>
#include <set>
#include <string>
#include <iostream>
#include "GrammarLoader.hpp"
#include "LRItem.hpp"
#include "LRAutomaton.hpp"
#include "FirstFollowCalculator.hpp"

using namespace std;

// SLR分析表动作类型
enum class SLRActionType {
	SHIFT,    // 移进
	REDUCE,   // 规约
	ACCEPT,   // 接受
	ERROR     // 错误
};

// SLR分析表动作
struct SLRAction {
	SLRActionType Type;
	int StateOrProduction;  // 状态编号（移进）或产生式编号（规约）

	// 构造函数
	SLRAction(SLRActionType type = SLRActionType::ERROR, int value = -1)
		: Type(type), StateOrProduction(value) {
	}

	// 转换为字符串
	string ToString() const {
		switch (Type) {
		case SLRActionType::SHIFT:
			return "S" + to_string(StateOrProduction);
		case SLRActionType::REDUCE:
			return "R" + to_string(StateOrProduction);
		case SLRActionType::ACCEPT:
			return "ACC";
		case SLRActionType::ERROR:
		default:
			return "-";
		}
	}
};

// SLR分析表生成器
struct SLRAnalysisTableBuilder {
	const LRAutomatonBuilder& AutomatonBuilder;
	const FirstFollowCalculator& FFCalculator;
	const GrammarDefinition& Grammar;

	// ACTION表：状态 × 终结符 → 动作
	map<pair<int, GrammarSymbol>, SLRAction> ActionTable;

	// GOTO表：状态 × 非终结符 → 状态
	map<pair<int, GrammarSymbol>, int> GotoTable;

	// 构造函数
	SLRAnalysisTableBuilder(const LRAutomatonBuilder& automatonBuilder,
		const FirstFollowCalculator& ffCalculator)
		: AutomatonBuilder(automatonBuilder),
		FFCalculator(ffCalculator),
		Grammar(automatonBuilder.AugmentedGrammar) {
		BuildTable();
	}

	// 构建分析表
	void BuildTable() {
		BuildActionTable();
		BuildGotoTable();
	}

	// 构建ACTION表
	void BuildActionTable() {
		// 遍历所有状态
		for (const LRState& State : AutomatonBuilder.States) {
			// 遍历状态中的所有项目
			for (const LRItem& Item : State.Items) {
				// 如果是接受项目
				if (Item.IsAcceptItem(Grammar.StartSymbol)) {
					// 在ACTION表中为该状态和$符号添加ACCEPT动作
					ActionTable[{State.StateId, FFCalculator.EndSymbol}] =
						SLRAction(SLRActionType::ACCEPT);
				}
				// 如果是规约项目
				else if (Item.IsReduceItem()) {
					// 遍历FOLLOW集，为每个终结符添加REDUCE动作
					const Production& Prod = Item.ProductionRef;
					const set<GrammarSymbol>& FollowSet =
						FFCalculator.GetFollowSet(Prod.Left);

					for (const GrammarSymbol& Term : FollowSet) {
						// 如果该位置已有动作，检查是否有冲突
						auto It = ActionTable.find({ State.StateId, Term });
						SLRAction NewAction(SLRActionType::REDUCE, Prod.Id);
						if (It != ActionTable.end()) {
							// 只有当新动作与已有动作不同时，才报告冲突
							if (It->second.Type != NewAction.Type || It->second.StateOrProduction != NewAction.StateOrProduction) {
								cout << "警告：在状态 " << State.StateId
									<< " 和符号 " << Term.Name
									<< " 处发现冲突：现有动作 "
									<< It->second.ToString()
									<< "，新动作 " << NewAction.ToString() << endl;
							}
						}
						else {
							// 添加规约动作
							ActionTable[{State.StateId, Term}] = NewAction;
						}
					}
				}
				// 如果是移进项目
				else {
					const GrammarSymbol* SymbolAfterDot = Item.GetSymbolAfterDot();
					if (SymbolAfterDot != nullptr) {
						// 如果是终结符，添加移进动作
						if (SymbolAfterDot->IsTerminal) {
							// 获取转移到的状态
							int NextState = State.GetTransition(SymbolAfterDot->Name);
							if (NextState != -1) {
								// 如果该位置已有动作，检查是否有冲突
								auto It = ActionTable.find({ State.StateId, *SymbolAfterDot });
								SLRAction NewAction(SLRActionType::SHIFT, NextState);
								if (It != ActionTable.end()) {
									// 只有当新动作与已有动作不同时，才报告冲突
									if (It->second.Type != NewAction.Type || It->second.StateOrProduction != NewAction.StateOrProduction) {
										cout << "警告：在状态 " << State.StateId
											<< " 和符号 " << SymbolAfterDot->Name
											<< " 处发现冲突：现有动作 "
											<< It->second.ToString()
											<< "，新动作 " << NewAction.ToString() << endl;
									}
								}
								else {
									// 添加移进动作
									ActionTable[{State.StateId, * SymbolAfterDot}] = NewAction;
								}
							}
						}
					}
				}
			}
		}
	}

	// 构建GOTO表
	void BuildGotoTable() {
		// 遍历所有状态
		for (const LRState& State : AutomatonBuilder.States) {
			// 遍历状态中的所有转移
			for (const auto& Transition : State.Transitions) {
				const string& SymbolName = Transition.first;
				int NextStateId = Transition.second;

				// 查找对应的符号
				for (const GrammarSymbol& NonTerminal : Grammar.NonTerminals) {
					if (NonTerminal.Name == SymbolName) {
						// 添加到GOTO表
						GotoTable[{State.StateId, NonTerminal}] = NextStateId;
						break;
					}
				}
			}
		}
	}

	// 获取ACTION
	const SLRAction& GetAction(int StateId, const GrammarSymbol& Symbol) const {
		static SLRAction ErrorAction;
		auto It = ActionTable.find({ StateId, Symbol });
		if (It != ActionTable.end()) {
			return It->second;
		}
		return ErrorAction;
	}

	// 获取GOTO
	int GetGoto(int StateId, const GrammarSymbol& Symbol) const {
		auto It = GotoTable.find({ StateId, Symbol });
		if (It != GotoTable.end()) {
			return It->second;
		}
		return -1;  // 错误
	}

	// 打印分析表
	void PrintTable() const {
		cout << endl << "SLR(1)分析表:" << endl;
		cout << "-------------------------------------------" << endl;

		// 获取所有终结符（包括$）和非终结符
		set<GrammarSymbol> Terminals;
		for (const auto& Term : Grammar.Terminals) {
			Terminals.insert(Term);
		}
		Terminals.insert(FFCalculator.EndSymbol);

		// 创建临时容器来存，以免干扰原数据
		set<GrammarSymbol> NonTerminals;
		for (const auto& NonTerm : Grammar.NonTerminals) {
			NonTerminals.insert(NonTerm);
		}

		// 移除增广文法的开始符号
		NonTerminals.erase(AutomatonBuilder.AugmentedGrammar.StartSymbol);

		// 打印表头
		cout << "状态\t|";
		for (const GrammarSymbol& Term : Terminals) {
			cout << "\t" << Term.Name;
		}
		for (const GrammarSymbol& NonTerm : NonTerminals) {
			cout << "\t" << NonTerm.Name;
		}
		cout << endl;

		cout << "--------|";
		for (size_t i = 0; i < Terminals.size() + NonTerminals.size(); i++) {
			cout << "--------";
		}
		cout << endl;

		// 打印每个状态的行
		for (const LRState& State : AutomatonBuilder.States) {
			cout << State.StateId << "\t|";

			// 打印ACTION表部分
			for (const GrammarSymbol& Term : Terminals) {
				const SLRAction& Action = GetAction(State.StateId, Term);
				cout << "\t" << Action.ToString();
			}

			// 打印GOTO表部分
			for (const GrammarSymbol& NonTerm : NonTerminals) {
				int GotoState = GetGoto(State.StateId, NonTerm);
				if (GotoState != -1) {
					cout << "\t" << GotoState;
				}
				else {
					cout << "\t-";
				}
			}

			cout << endl;
		}

		cout << "-------------------------------------------" << endl;
	}


};

#endif // SLRANALYSISTABLE_HPP