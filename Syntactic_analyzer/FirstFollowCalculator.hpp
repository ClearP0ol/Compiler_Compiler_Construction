#ifndef FIRSTFOLLOWCALCULATOR_HPP
#define FIRSTFOLLOWCALCULATOR_HPP

#include "GrammarLoader.hpp"
#include <map>
#include <set>

using namespace std;

struct FirstFollowCalculator
{
	const GrammarDefinition &Grammar;
	map<GrammarSymbol, set<GrammarSymbol>> FirstSets;
	map<GrammarSymbol, set<GrammarSymbol>> FollowSets;

	// 特殊符号：ε（空串）和 $（输入结束符）
	GrammarSymbol EpsilonSymbol;
	GrammarSymbol EndSymbol;

	// 构造函数
	FirstFollowCalculator(const GrammarDefinition &grammar)
		: Grammar(grammar)
	{
		// 初始化特殊符号
		EpsilonSymbol = GrammarSymbol("ε", true);
		EndSymbol = GrammarSymbol("$", true);
	}

	// 计算FIRST集和FOLLOW集
	void Calculate()
	{
		CalculateFirstSets();
		CalculateFollowSets();
	}

	// 获取FIRST集
	const set<GrammarSymbol> &GetFirstSet(const GrammarSymbol &symbol) const
	{
		static set<GrammarSymbol> EmptySet;
		auto It = FirstSets.find(symbol);
		if (It != FirstSets.end())
		{
			return It->second;
		}
		return EmptySet;
	}

	// 获取FOLLOW集
	const set<GrammarSymbol> &GetFollowSet(const GrammarSymbol &symbol) const
	{
		static set<GrammarSymbol> EmptySet;
		auto It = FollowSets.find(symbol);
		if (It != FollowSets.end())
		{
			return It->second;
		}
		return EmptySet;
	}

	// 获取字符串的FIRST集（用于产生式右部）
	set<GrammarSymbol> GetFirstSetForSequence(const vector<GrammarSymbol> &sequence) const
	{
		set<GrammarSymbol> Result;

		// 如果序列为空，返回包含ε的集合
		if (sequence.empty())
		{
			Result.insert(EpsilonSymbol);
			return Result;
		}

		bool AllCanBeEpsilon = true;

		// 遍历每个符号
		for (const auto &Symbol : sequence)
		{
			const auto &FirstOfSymbol = GetFirstSet(Symbol);

			// 添加当前符号的FIRST集（除了ε）
			for (const auto &FirstSym : FirstOfSymbol)
			{
				if (FirstSym.Name != "ε")
				{
					Result.insert(FirstSym);
				}
			}

			// 检查当前符号是否能推出ε
			bool CanBeEpsilon = false;
			for (const auto &FirstSym : FirstOfSymbol)
			{
				if (FirstSym.Name == "ε")
				{
					CanBeEpsilon = true;
					break;
				}
			}

			if (!CanBeEpsilon)
			{
				AllCanBeEpsilon = false;
				break;
			}
		}

		// 如果所有符号都能推出ε，则添加ε
		if (AllCanBeEpsilon)
		{
			Result.insert(EpsilonSymbol);
		}

		return Result;
	}

	// 打印FIRST集
	void PrintFirstSets() const
	{
		cout << "\nFIRST 集合：" << endl;

		// 打印非终结符
		for (const auto &NT : Grammar.NonTerminals)
		{
			cout << "FIRST(" << NT.Name << ") = { ";
			auto It = FirstSets.find(NT);
			if (It != FirstSets.end())
			{
				bool First = true;
				for (const auto &Sym : It->second)
				{
					if (!First)
						cout << ", ";
					cout << Sym.Name;
					First = false;
				}
			}
			cout << " }" << endl;
		}

		// 打印终结符
		for (const auto &T : Grammar.Terminals)
		{
			cout << "FIRST(" << T.Name << ") = { ";
			auto It = FirstSets.find(T);
			if (It != FirstSets.end())
			{
				bool First = true;
				for (const auto &Sym : It->second)
				{
					if (!First)
						cout << ", ";
					cout << Sym.Name;
					First = false;
				}
			}
			cout << " }" << endl;
		}
	}

	// 打印FOLLOW集
	void PrintFollowSets() const
	{
		cout << "\nFOLLOW 集合：" << endl;

		for (const auto &NT : Grammar.NonTerminals)
		{
			cout << "FOLLOW(" << NT.Name << ") = { ";
			auto It = FollowSets.find(NT);
			if (It != FollowSets.end())
			{
				bool First = true;
				for (const auto &Sym : It->second)
				{
					if (!First)
						cout << ", ";
					cout << Sym.Name;
					First = false;
				}
			}
			cout << " }" << endl;
		}
	}

	// 计算FIRST集
	void CalculateFirstSets()
	{
		bool Changed = true;

		// 所有终结符的FIRST集就是它自己
		for (const auto &Terminal : Grammar.Terminals)
		{
			set<GrammarSymbol> FirstSet;
			FirstSet.insert(Terminal);
			FirstSets[Terminal] = FirstSet;
		}

		// 所有非终结符的FIRST集为空
		for (const auto &NonTerminal : Grammar.NonTerminals)
		{
			FirstSets[NonTerminal] = set<GrammarSymbol>();
		}

		// 迭代计算直到不再变化
		while (Changed)
		{
			Changed = false; // 每次循环开始时重置为false

			for (const auto &Production : Grammar.Productions)
			{
				const GrammarSymbol &Left = Production.Left;
				const vector<GrammarSymbol> &Right = Production.Right;

				set<GrammarSymbol> &FirstOfLeft = FirstSets[Left];

				// 如果产生式右部为空，添加ε
				if (Right.empty())
				{
					if (FirstOfLeft.insert(EpsilonSymbol).second)
					{
						Changed = true;
					}
					continue;
				}

				// 遍历右部符号
				bool AllCanBeEpsilon = true;
				for (const auto &Symbol : Right)
				{
					const set<GrammarSymbol> &FirstOfSymbol = FirstSets[Symbol];

					// 添加当前符号的FIRST集（除了ε）到左部FIRST集
					for (const auto &FirstSym : FirstOfSymbol)
					{
						if (FirstSym.Name != "ε")
						{
							if (FirstOfLeft.insert(FirstSym).second)
							{
								Changed = true;
							}
						}
					}

					// 检查当前符号是否能推出ε
					bool CanBeEpsilon = false;
					for (const auto &FirstSym : FirstOfSymbol)
					{
						// 如果能推出
						if (FirstSym.Name == "ε")
						{
							CanBeEpsilon = true;
							break;
						}
					}

					// 如果右部至少有一个符号的FIRST集合不是空
					if (!CanBeEpsilon)
					{
						AllCanBeEpsilon = false;
						break;
					}
				}

				// 如果所有符号都能推出ε，添加ε
				if (AllCanBeEpsilon)
				{
					if (FirstOfLeft.insert(EpsilonSymbol).second)
					{
						Changed = true;
					}
				}
			}
		}
	}

	// 计算FOLLOW集
	void CalculateFollowSets()
	{
		bool Changed = true; // 初始化为true

		// 初始化所有非终结符的FOLLOW集为空
		for (const auto &NonTerminal : Grammar.NonTerminals)
		{
			FollowSets[NonTerminal] = set<GrammarSymbol>();
		}

		// 开始符号的FOLLOW集包含$
		FollowSets[Grammar.StartSymbol].insert(EndSymbol);

		// 迭代计算直到不再变化
		while (Changed)
		{
			Changed = false; // 每次循环开始时重置为false

			for (const auto &Production : Grammar.Productions)
			{
				const GrammarSymbol &Left = Production.Left;
				const vector<GrammarSymbol> &Right = Production.Right;

				// 遍历右部的每个非终结符
				for (size_t I = 0; I < Right.size(); I++)
				{
					// 如果是非终结符
					if (!Right[I].IsTerminal)
					{
						set<GrammarSymbol> &FollowOfB = FollowSets[Right[I]];

						// 若B后面有符号：A→αBβ
						if (I + 1 < Right.size())
						{

							// 计算后面符号串的FIRST集
							vector<GrammarSymbol> Beta(Right.begin() + I + 1, Right.end());
							set<GrammarSymbol> FirstOfBeta = GetFirstSetForSequence(Beta);

							// 添加到FOLLOW(B)中（除了ε）
							for (const auto &Sym : FirstOfBeta)
							{
								if (Sym.Name != "ε")
								{
									if (FollowOfB.insert(Sym).second)
									{
										Changed = true;
									}
								}
							}

							// 如果β能推出ε，添加FOLLOW(A)到FOLLOW(B)
							bool BetaCanBeEpsilon = false;
							for (const auto &Sym : FirstOfBeta)
							{
								if (Sym.Name == "ε")
								{
									BetaCanBeEpsilon = true;
									break;
								}
							}

							if (BetaCanBeEpsilon)
							{
								const set<GrammarSymbol> &FollowOfA = FollowSets[Left];
								for (const auto &Sym : FollowOfA)
								{
									if (FollowOfB.insert(Sym).second)
									{
										Changed = true;
									}
								}
							}
						}
						// B是最后一个符号，A→αB
						else
						{
							// 添加FOLLOW(A)到FOLLOW(B)
							const set<GrammarSymbol> &FollowOfA = FollowSets[Left];
							for (const auto &Sym : FollowOfA)
							{
								if (FollowOfB.insert(Sym).second)
								{
									Changed = true;
								}
							}
						}
					}
				}
			}
		}
	}
};

#endif // FIRSTFOLLOWCALCULATOR_HPP