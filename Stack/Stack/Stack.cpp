// Stack.cpp : Defines the entry point for the console application.
//

#include "stdafx.h"
#include <iostream>
#include <atomic>
#include <list>
#include <thread>
#include <iterator>
#include <algorithm>

using namespace std;

template<typename TimestampedItem>
struct Node {
	TimestampedItem item;
	Node<TimestampedItem> *next;
	bool taken;
};

template<typename TimestampedItem>
class SPBuffer {
public:

	tuple<Node<TimestampedItem>*, int> top;

	Node<TimestampedItem>* createNode(TimestampedItem _item, bool _taken) {
		
		Node<TimestampedItem> *newNode;
		newNode = (Node<TimestampedItem> *)malloc(sizeof(Node<TimestampedItem>));
		newNode->item = _item;
		newNode->taken = _taken;

		return newNode;
	}

	void init() {
		Node<TimestampedItem> *sentinel = createNode(NULL, true);
		sentinel->next = sentinel;
		top = make_tuple(sentinel, 0);
	}
	
	void ins(TimestampedItem item) {
		Node<TimestampedItem> *newNode = createNode(item, false);
		tuple<Node<TimestampedItem>*, int> topMost = top;
		while (get<0>(topMost)->next != get<0>(topMost) && get<0>(topMost)->taken)
		{
			get<0>(topMost) = get<0>(topMost)->next;
		}
		newNode->next = get<0>(topMost);
		top = make_tuple(newNode, get<1>(topMost) + 1);
	}

	/*
	tuple<Node<TimestampedItem>*, Node<TimIteestampedm>*, int> getSP()
	{
		tuple<Node<TimestampedItem>*, int> oldTop = top;
		Node<TimestampedItem> *result = get<0>(oldTop);
		while (true)
		{
			if (!result->taken)
			{
				return make_tuple(result, get<0>(oldTop), get<1>(oldTop));
			}
			else if (result->next == result)
			{
				return make_tuple(nullptr, get<0>(oldTop), get<1>(oldTop));
			}
			else
			{
				result = result->next;
			}
		}
	}

	bool tryRemSP(tuple<Node<TimestampedItem>*, int> oldTop, Node<TimestampedItem>* node)
	{
		atomic<bool> taken = node->taken;
		bool _false = false;
		if (taken.compare_exchange_weak(_false, true))
		{
			atomic<Node<TimestampedItem>*> top = get<0>(this->top);
			top.compare_exchange_weak(get<0>(oldTop), node);
			return true;
		}
		return false;
	}

	void printTop()
	{
		cout << get<0>(top)->item << endl;
	}
	
	void printRemove()
	{
		tuple<Node<TimestampedItem>*, int> oldTop = make_tuple(get<0>(top)->next, 0);
		tryRemSP(top, get<0>(top)->next);
		cout << get<0>(top)->item << endl;
	}
	*/
};

int main()
{
	SPBuffer<int> sp;
	sp.init();
	sp.ins(2);
	//sp.ins(3);
	//sp.ins(54);
	//sp.printTop();
	//sp.printRemove();
	system("pause");
	return 0;
}

