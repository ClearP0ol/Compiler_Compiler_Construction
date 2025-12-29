#ifndef LRAUTOMATON_HPP
#define LRAUTOMATON_HPP

#include "FirstFollowCalculator.hpp"
#include "LRItem.hpp"
#include <queue>

using namespace std;

// LR状态
struct LRState {
	int StateId;                           // 状态编号
	set<LRItem> Items;                     // 项目集
	map<string, int> Transitions;          // GOTO函数：符号 -> 下一状态

	// 构造函数
	LRState() : StateId(-1) {}
	LRState(int Id, const set<LRItem>& Items) : StateId(Id), Items(Items) {}

	// 添加转移
	void AddTransition(const string& SymbolName, int NextStateId) {
		if (NextStateId < 0) {
			throw invalid_argument("无效的下一状态ID: " + to_string(NextStateId));
		}
		Transitions[SymbolName] = NextStateId;
	}

	// 获取转移状态
	int GetTransition(const string& SymbolName) const {
		auto It = Transitions.find(SymbolName);
		if (It != Transitions.end()) {
			return It->second;
		}
		return -1;  // 无转移
	}

	// 打印状态
	void Print() const {
		cout << "状态 " << StateId << ":" << endl;

		// 添加项目集
		for (const auto& Item : Items) {
			cout << "  " << Item.ToString() << endl;
		}

		// 添加转移信息
		if (!Transitions.empty()) {
			cout << "  转移:" << endl;
			for (const auto& Trans : Transitions) {
				cout << "    在 " << Trans.first << " 上转到状态 " << Trans.second << endl;
			}
		}

		cout << endl;
	}
};

// LR(0)自动机构建器
struct LRAutomatonBuilder {
	const GrammarDefinition& OriginalGrammar;  // 原始语法
	GrammarDefinition AugmentedGrammar;        // 增广语法
	vector<LRState> States;                    // 所有状态
	map<set<LRItem>, int> StateMap;           // 项目集 -> 状态ID映射
	int NextStateId;                          // 下一个状态ID

	// 构造函数
	LRAutomatonBuilder(const GrammarDefinition& Grammar)
		: OriginalGrammar(Grammar), NextStateId(0) {
		// 创建增广语法
		CreateAugmentedGrammar();
		Build();
	}

	// 构建LR(0)自动机
	void Build() {
		// 清空之前的状态
		States.clear();
		StateMap.clear();
		NextStateId = 0;

		// 确保有增广语法
		if (AugmentedGrammar.Productions.empty()) {
			throw runtime_error("增广语法为空，无法构建自动机");
		}

		// 创建初始项目集
		set<LRItem> InitialItems = GetInitialItems();
		int InitialStateId = AddState(InitialItems);

		// 使用队列进行广度优先搜索
		queue<int> StateQueue;
		StateQueue.push(InitialStateId);

		while (!StateQueue.empty()) {
			int CurrentStateId = StateQueue.front();
			StateQueue.pop();

			// 收集所有可能的转移符号（圆点后的符号）
			map<string, set<LRItem>> SymbolTransitions;

			for (const auto& Item : States[CurrentStateId].Items) {
				const GrammarSymbol* NextSymbol = Item.GetSymbolAfterDot();
				if (NextSymbol != nullptr) {
					// 对于每个圆点后的符号，收集可转移的项目
					LRItem NextItem = Item.GetNextItem();
					SymbolTransitions[NextSymbol->Name].insert(NextItem);
				}
			}

			// 对每个转移符号，计算闭包并创建新状态
			for (const auto& TransPair : SymbolTransitions) {
				const string& SymbolName = TransPair.first;
				const set<LRItem>& KernelItems = TransPair.second;

				// 计算闭包
				set<LRItem> NewItemSet = Closure(KernelItems);

				// 检查是否已存在相同项目集的状态
				int TargetStateId = FindStateId(NewItemSet);
				if (TargetStateId == -1) {
					// 新状态
					TargetStateId = AddState(NewItemSet);
					StateQueue.push(TargetStateId);
				}

				// 添加转移
				States[CurrentStateId].AddTransition(SymbolName, TargetStateId);
			}
		}
	}

	// 判断语法是否已经被增广
	bool IsGrammarAlreadyAugmented(const GrammarDefinition& Grammar) const {
		// 如果开始符号以 ' 结尾，说明已经增广
		if (!Grammar.StartSymbol.Name.empty() &&
			Grammar.StartSymbol.Name.back() == '\'') {
			return true;
		}

		// 检查是否存在增广产生式 S' -> S
		string ExpectedAugSymbol = Grammar.StartSymbol.Name + "'";
		for (const auto& Prod : Grammar.Productions) {
			if (Prod.Left.Name == ExpectedAugSymbol &&
				Prod.Right.size() == 1 &&
				Prod.Right[0].Name == Grammar.StartSymbol.Name) {
				return true;
			}
		}

		return false;
	}

	// 获取特定状态
	const LRState& GetState(int StateId) const {
		if (StateId < 0 || StateId >= (int)States.size()) {
			throw out_of_range("无效的状态ID: " + to_string(StateId));
		}
		return States[StateId];
	}

	// 打印整个自动机
	void PrintAutomaton() const {
		cout << "\nLR(0)自动机" << endl;
		cout << "状态总数: " << States.size() << endl;
		cout << "增广语法开始符号: " << AugmentedGrammar.StartSymbol.Name << endl;

		for (const auto& State : States) {
			State.Print();
		}
	}

	// 创建增广语法（添加新的开始符号 S' -> S）
	void CreateAugmentedGrammar() {
		// 检查是否已经是增广语法
		if (IsGrammarAlreadyAugmented(OriginalGrammar)) {
			AugmentedGrammar = OriginalGrammar;
			cout << "语法已经是增广形式。" << endl;
			return;
		}

		// 复制原始语法
		AugmentedGrammar = OriginalGrammar;

		// 创建新的开始符号
		GrammarSymbol NewStart(OriginalGrammar.StartSymbol.Name + "'", false);
		AugmentedGrammar.StartSymbol = NewStart;

		// 添加新的开始符号到非终结符
		AugmentedGrammar.NonTerminals.push_back(NewStart);

		// 添加新的产生式：S' -> S
		Production NewProd;
		NewProd.Left = NewStart;
		NewProd.Right.push_back(OriginalGrammar.StartSymbol);
		NewProd.Id = 0;  // 设置为第一个产生式

		// 插入到产生式列表的开头
		AugmentedGrammar.Productions.insert(
			AugmentedGrammar.Productions.begin(),
			NewProd
		);

		// 重新编号所有产生式
		for (size_t i = 0; i < AugmentedGrammar.Productions.size(); i++) {
			AugmentedGrammar.Productions[i].Id = static_cast<int>(i);
		}

		cout << "已创建增广语法，新开始符号: " << NewStart.Name << endl;
	}

	// 获取初始项目集
	set<LRItem> GetInitialItems() const {
		set<LRItem> Items;

		// 找到所有以增广开始符号为左部的产生式
		for (const auto& Prod : AugmentedGrammar.Productions) {
			if (Prod.Left.Name == AugmentedGrammar.StartSymbol.Name) {
				Items.insert(LRItem(Prod, 0));
			}
		}

		return Closure(Items);
	}

	// 计算项目集的闭包
	set<LRItem> Closure(const set<LRItem>& Items) const {
		set<LRItem> ClosureSet = Items;
		bool Changed;

		do {
			Changed = false;
			set<LRItem> NewItems;

			// 对于ClosureSet中的每个项目 A -> α•Bβ
			for (const auto& Item : ClosureSet) {
				const GrammarSymbol* SymbolAfterDot = Item.GetSymbolAfterDot();
				if (SymbolAfterDot != nullptr && !SymbolAfterDot->IsTerminal) {
					// B是非终结符，添加所有 B -> •γ 到闭包
					for (const auto& Prod : AugmentedGrammar.Productions) {
						if (Prod.Left.Name == SymbolAfterDot->Name) {
							LRItem NewItem(Prod, 0);
							if (ClosureSet.find(NewItem) == ClosureSet.end() &&
								NewItems.find(NewItem) == NewItems.end()) {
								NewItems.insert(NewItem);
								Changed = true;
							}
						}
					}
				}
			}

			// 添加新项目到闭包
			for (const auto& Item : NewItems) {
				ClosureSet.insert(Item);
			}

		} while (Changed);

		return ClosureSet;
	}

	// 添加新状态
	int AddState(const set<LRItem>& Items) {
		int StateId = NextStateId++;
		States.emplace_back(StateId, Items);
		StateMap[Items] = StateId;
		return StateId;
	}

	// 获取项目集对应的状态ID（如果存在）
	int FindStateId(const set<LRItem>& Items) const {
		auto It = StateMap.find(Items);
		if (It != StateMap.end()) {
			return It->second;
		}
		return -1;  // 不存在
	}
};

#endif // LRAUTOMATON_HPP