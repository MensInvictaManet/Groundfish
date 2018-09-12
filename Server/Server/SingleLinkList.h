#ifndef _SLLIST_H_
#define _SLLIST_H_

#include <cstring>

template <typename Type>
class SLList
{
private:
	struct Node
	{
	public:
		Type	Data;
		Node*	Next;
	};
	Node*	Head;
public:
	SLList(void);
	~SLList(void);

	void	AddHead(Type v);
	void	Clear(void);
	bool	Remove(int Index);
	int		Size(void);
	Type&	operator[](int index);
};

template <typename Type>
SLList<Type>::SLList(void)
{
	Head = 0;
}

template <typename Type>
SLList<Type>::~SLList(void)
{
	Clear();
}

template <typename Type>
void SLList<Type>::AddHead(Type v)
{
	if (Head == 0)
	{
		Head = new Node;
		Head->Next = 0;
	}
	else
	{
		Node* N = Head;
		Head = new Node;
		Head->Next = N;
	}

	memcpy(&Head->Data, &v, sizeof(Type));
}

template <typename Type>
void SLList<Type>::Clear(void)
{
	Node* N = Head;
	Node* NextNode;

	while (N != 0)
	{
		NextNode = N->Next;
		delete N;
		N = NextNode;
	}
}

template <typename Type>
bool SLList<Type>::Remove(int Index)
{
	if (Index == 0)
	{
		Head = Head->Next;
		return true;
	}

	Node* LinkNode;
	Node* RemovalNode = Head;
	for (int i = 1; i < Index; i++) RemovalNode = RemovalNode->Next;

	if (RemovalNode->Next == 0) return false;

	LinkNode = RemovalNode;
	RemovalNode = RemovalNode->Next;
	LinkNode->Next = RemovalNode->Next;

	delete RemovalNode;
	return true;
}

template <typename Type>
int SLList<Type>::Size(void)
{
	int i;
	Node* N = Head;
	for (i = 0; N != 0; i += 1) N = N->Next;
	return i;
}

template <typename Type>
Type& SLList<Type>::operator[](int Index)
{
	Node* N = Head;
	for (int i = 0; i < Index; i++) N = N->Next;
	return N->Data;
}

#endif