#include <sys/types.h>
#include <unistd.h>
#include <iostream>
#include <map>
#include <unordered_map>

#include <queue>

using namespace std;


class Node {
    friend class Node;

    public:
    Node(int val) {
        this->val = val;
        this->left = nullptr;
        this->right = nullptr;
    }

    Node(int val, Node* left, Node* right) {
        this->val = val;
        this->left = left;
        this->right = right;
    }

    public:
    Node* left;
    Node* right;
    int val;
};

int visited(Node *root) {
    std::queue<Node*> q;
    q.push(root);

    int end = 1;
    int inqueue = 1;
    int visited = 0;
    int height = 0;

    while (!q.empty() ) {
        Node *p = q.front();
        q.pop();

        visited++;

//        std::cout << p->val << std::endl;

        if (p->left != nullptr) {
            q.push(p->left);
            inqueue++;
        }

        if (p->right != nullptr) {
            q.push(p->right);
            inqueue++;
        }

        if (end == visited) {
            height++;
            end = inqueue;
        }
    }

    return height;
}

int calculateTreeHeight(Node *root)
{
	if(root == NULL)
		return 0;
	int visitedNumber = 0;
	int enQueNumber = 1; //当前入栈的序号，root为1
	int lastLevelNumber = 1;
	int height = 0;

	queue<Node*> q;
	q.push(root);
	while(!q.empty())
	{
		Node *node = q.front();
		q.pop();
		visitedNumber++;

		if(node->left != NULL)
		{
			q.push(node->left);
			enQueNumber++;
		}
		if(node->right != NULL)
		{
			q.push(node->right);
			enQueNumber++;
		}

		//访问到当前level上的最后一个节点
		if(visitedNumber == lastLevelNumber)
		{
			height++;
			lastLevelNumber = enQueNumber;
		}
	}
	return height;
}


/***
 *
    通过层次遍历来解决，
    层次遍历的特点是，遍历完一层之后，下一层的节点已经全部进入队列，
    每层的最后一个节点，就是在上层入队列的最后一个节点
    假设，按照遍历顺序对节点编号。
    遍历过程中遇到结尾节点，说明该层遍历结束
    遍历完成一层，累加层数，更新下一层的结束接的编号
 * */


int main() {
    Node root(0,
            new Node(1),
            new Node(2,
                new Node(3),
                new Node(4,
                    new Node(5),
                    nullptr)));

    int a = calculateTreeHeight(&root);

    std::cout << a << std::endl;

    std::cout << visited(&root) << std::endl;
}

int main1() {

//    pid_t pid1;
//    pid_t pid2;
//
//    pid1 = fork();
//    pid2 = fork();
//
//    cout << "cur pid:"<<getpid() << ",pid1:" << pid1 << ",pid2:" << pid2 << endl;


    std::map<int, int> m;
    m[1] = 1;
    m[2] = 2;
    m[3] = 3;
    m[4] = 4;

    for (auto p = m.begin(); p != m.end(); ) {
        p = m.erase(p);
    }

    std::cout << "map.size=" << m.size() << std::endl;

    std::unordered_map<int, int> mp;
    mp[10] = 1;
    for (auto p = mp.begin(); p != mp.end(); ++p) {
        std::cout << p->first << ":" << p->second << std::endl;
    }

    return 0;

}

/*
 *
cur pid:8643,pid1:8645,pid2:8646
cur pid:8646,pid1:8645,pid2:0
cur pid:8645,pid1:0,pid2:8647
cur pid:8647,pid1:0,pid2:0

推导过程
cur -> pid1 -> pid2
            -> 0
    -> 0    -> pid2
            -> 0


cur pid1 pid2
cur pid1 0
cur 0 pid2
cur 0 0
 */


