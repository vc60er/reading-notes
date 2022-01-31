#include <iostream>

#include <tuple>

using namespace std;

class Node {
public:
    Node(int val) {
        this->val = val;
        this->next = nullptr;
    }
    Node(int val, Node* next) {
        this->val = val;
        this->next = next;
    }


public:
    int val;
    Node* next;
};

auto test(Node* head, int x)  {

    int y = x / 2;

    Node* p = head;
    Node* q = head;

    while (p != nullptr && q != nullptr) {

       // std::cout << "p:" << p->val << " q:" << q->val << " " << y << std::endl;


        if (q->next != nullptr)  {
            q = q->next->next;
        } else {
            break;
        }

       if (y > 0) {
            y--;
       } else {
            p = p->next;
       }
    }


   Node* start = p;
   y = x;

   while (y > 0 && p != nullptr)  {
        y--;
        p = p->next;
   }
   Node* end = p;


    return std::make_tuple(start, end);
}

int main() {
    int a[] = {1,2,3,4,5,6,7};
    Node* head = nullptr;
    Node* tail = nullptr;

    for (auto n : a) {
        Node* p = new Node(n);
        if (head == nullptr) {
            head = p;
            tail = p;
        } else {
            tail->next = p;
            tail = p;
        }
    }

    for (auto p = head; p != nullptr; p=p->next) {
        std::cout << p->val << std::endl;
    }

    auto [start, end] = test(head, 3);


   std::cout << "output:" << std::endl;

    for (auto p = start; p != end && p != nullptr; p=p->next) {
        std::cout << p->val << std::endl;
    }

    return 0;
}
