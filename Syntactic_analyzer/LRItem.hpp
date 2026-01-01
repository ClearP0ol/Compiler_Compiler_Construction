#ifndef LRITEM_HPP
#define LRITEM_HPP

#include "GrammarLoader.hpp"

using namespace std;

struct LRItem
{
	Production ProductionRef;
	size_t DotPosition; // 0表示圆点在第一个符号前

	// 构造函数
	LRItem() : DotPosition(0) {}

	LRItem(const Production &prod, size_t dotPos = 0)
		: ProductionRef(prod), DotPosition(dotPos)
	{
	}

	// 获取圆点后的符号
	const GrammarSymbol *GetSymbolAfterDot() const
	{
		if (DotPosition < ProductionRef.Right.size())
		{
			return &ProductionRef.Right[DotPosition]; // 返回指针
		}
		return nullptr; // 没有符号后返回空指针
	}

	// 获取下一个项目（圆点向前移动一位）
	LRItem GetNextItem() const
	{
		if (DotPosition < ProductionRef.Right.size())
		{
			return LRItem(ProductionRef, DotPosition + 1);
		}
		return *this; // 已经是规约项目
	}

	// 检查是否为规约项目
	bool IsReduceItem() const
	{
		return DotPosition >= ProductionRef.Right.size();
	}

	// 检查是否为接受项目（针对增广文法）
	bool IsAcceptItem(const GrammarSymbol &StartSymbol) const
	{
		// 检查是否为 S' -> S• 这样的项目
		if (ProductionRef.Left.Name == StartSymbol.Name + "'" &&
			ProductionRef.Right.size() == 1 &&
			ProductionRef.Right[0].Name == StartSymbol.Name &&
			DotPosition == 1)
		{
			return true;
		}
		return false;
	}

	// 用于set排序
	bool operator<(const LRItem &Other) const
	{
		// 先比较左部
		if (ProductionRef.Left.Name != Other.ProductionRef.Left.Name)
		{
			return ProductionRef.Left.Name < Other.ProductionRef.Left.Name;
		}

		// 比较右部长度
		if (ProductionRef.Right.size() != Other.ProductionRef.Right.size())
		{
			return ProductionRef.Right.size() < Other.ProductionRef.Right.size();
		}

		// 比较圆点位置
		if (DotPosition != Other.DotPosition)
		{
			return DotPosition < Other.DotPosition;
		}

		// 比较右部符号
		for (size_t i = 0; i < ProductionRef.Right.size(); ++i)
		{
			if (ProductionRef.Right[i].Name != Other.ProductionRef.Right[i].Name)
			{
				return ProductionRef.Right[i].Name < Other.ProductionRef.Right[i].Name;
			}
		}
		return false;
	}

	// 相等比较
	bool operator==(const LRItem &Other) const
	{
		if (ProductionRef.Left.Name != Other.ProductionRef.Left.Name)
			return false;
		if (ProductionRef.Right.size() != Other.ProductionRef.Right.size())
			return false;
		if (DotPosition != Other.DotPosition)
			return false;

		for (size_t i = 0; i < ProductionRef.Right.size(); ++i)
		{
			if (ProductionRef.Right[i].Name != Other.ProductionRef.Right[i].Name)
			{
				return false;
			}
		}
		return true;
	}

	// 不相等比较
	bool operator!=(const LRItem &Other) const
	{
		return !(*this == Other);
	}

	// 转换为字符串
	string ToString() const
	{
		string Result = ProductionRef.Left.Name + " -> ";
		for (size_t i = 0; i < ProductionRef.Right.size(); ++i)
		{
			if (i == DotPosition)
				Result += "• ";
			Result += ProductionRef.Right[i].Name + " ";
		}
		if (DotPosition == ProductionRef.Right.size())
		{
			Result += "•";
		}
		return Result;
	}

	// 获取项目的哈希值
	size_t Hash() const
	{
		size_t HashValue = hash<string>{}(ProductionRef.Left.Name);
		HashValue ^= hash<size_t>{}(ProductionRef.Right.size()) + 0x9e3779b9 + (HashValue << 6) + (HashValue >> 2);
		HashValue ^= hash<size_t>{}(DotPosition) + 0x9e3779b9 + (HashValue << 6) + (HashValue >> 2);

		for (const auto &Symbol : ProductionRef.Right)
		{
			HashValue ^= hash<string>{}(Symbol.Name) + 0x9e3779b9 + (HashValue << 6) + (HashValue >> 2);
		}

		return HashValue;
	}
};

// 哈希函数特化，用于unordered_set
namespace std
{
	template <>
	struct hash<LRItem>
	{
		size_t operator()(const LRItem &Item) const
		{
			return Item.Hash();
		}
	};
}

#endif // LRITEM_HPP