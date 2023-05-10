#include <iostream>
#include "cstdlib"
#include <cmath>
#include <cstring>
#include <mutex>
#include <fstream>

#define STORE_FILE_PATH "store/dumpFile"

std::mutex mtx;                 // 互斥锁，用于多线程操作共享数据
std::string delimiter = ":";    // 用于分割key和value的分隔符

// 跳表节点
template<typename K, typename V>    // K为key的类型，V为value的类型
class Node{

public:
    Node() {}               // 默认构造函数

    Node(K k, V v, int);    // 构造函数

    ~Node();                // 析构函数

    //函数名后面加了const修饰符，这样说明函数的成员对象是不允许修改的
    K get_key() const;      // 获取key

    V get_value() const;    // 获取value

    void set_value(V v);    // 设置value


    Node(K, V) **forward;   //用于保存指向不同级别的下一节点的指针的线性数组

    int node_level;         // 节点的层数

private:
    K key;                  // 键值
    V value;                // 值
};

template<typename K, typename V>
Node<K, V>::Node(K k, V v, int level) {
    this->key = k;
    this->value = v;
    this->node_level = level;

    // 创建一个指针数组，数组大小为level
    this->forward = new Node<K,V>*[level+1];

    // 将数组初始化为0
    memset(this->forward, 0, sizeof(Node<K,V>*)*(level+1));
}

template<typename K, typename V>
Node<K, V>::~Node() {
    delete []forward;    // 释放指针数组
}

template<typename K, typename V>
K Node<K, V>::get_key() const {
    return key;
}

template<typename K, typename V>
V Node<K, V>::get_value() const {
    return value;
}

template<typename K, typename V>
void Node<K, V>::set_value(V v) {
    this->value = v;
}

// 跳表，本质上是一个链表，只不过每个节点有多个指针，指向不同层级的下一个节点
template<typename K, typename V>
class SkipList{

public:
    SkipList(int);                          // 构造函数
    ~SkipList();                            // 析构函数
    int get_random_level();                 // 获取随机层数
    Node<K,V>* create_node(K, V, int);      // 创建节点
    int insert_element(K, V);               // 插入节点
    void display_list();                    // 显示跳表
    bool search_element(K);                 // 查找节点
    void delete_element(K);                 // 删除节点
    void dump_file();                       // 将跳表数据存储到文件
    void load_file();                       // 从文件中加载跳表数据
    int size();                             // 获取跳表大小

private:
    // 从字符串中获取key和value
    void get_key_value_from_string(const std::string& str, std::string* key, std::string* value);
    bool is_valid_key(const std::string& key);    // 判断key是否合法

private:
    int max_level;                              // 跳表的最大层数
    int skip_list_level;                        // 跳表的当前层数
    Node<K,V> *head_node;                       // 跳表的头节点
    int element_count;                          // 跳表的元素个数

    std::ofstream _file_writer;                 // 文件输出流
    std::ifstream _file_reader;                 // 文件输入流
};

// 构造函数
template<typename K, typename V>
SkipList<K, V>::SkipList(int max_level) {

    this->max_level = max_level;
    this->skip_list_level = 0;
    this->element_count = 0;

    //创建头节点，并将key和value初始化为null
    K k = NULL;
    V v = NULL;
    this->head_node = new Node<k, V>(k,v,max_level);
}

// 析构函数
template<typename K, typename V>
SkipList<K, V>::~SkipList() {

    if (_file_writer.is_open()) {
        _file_writer.close();
    }
    if (_file_reader.is_open()) {
        _file_reader.close();
    }
    delete _header;
}

// 获取随机层数
template<typename K, typename V>
int SkipList<K, V>::get_random_level(){

    int k = 1;
    while (rand() % 2) {    // 生成随机数
        k++;
    }
    k = (k < _max_level) ? k : _max_level;  // 如果随机数大于最大层数，则返回最大层数
    return k;
}

template<typename K, typename V>
Node<K, V>* SkipList<K,V>::create_node(const K key, const V value, int level) {
    Node<K,V> *n = new Node<K,V>(key, value, level);
    return n;
}


// 在跳表中插入给定的键和值
// return 1表示元素存在
// r返回0表示插入成功
/*
                           +------------+
                           |  insert 50 |
                           +------------+
level 4     +-->1+                                                      100
                 |
                 |                      insert +----+
level 3         1+-------->10+---------------> | 50 |          70       100
                                               |    |
                                               |    |
level 2         1          10         30       | 50 |          70       100
                                               |    |
                                               |    |
level 1         1    4     10         30       | 50 |          70       100
                                               |    |
                                               |    |
level 0         1    4   9 10         30   40  | 50 |  60      70       100
                                               +----+
update[] = {40，30，30，10，1}
*/

template<typename K, typename V>
int SkipList<K, V>::insert_element(const K key, const V value) {
    mtx.lock();     // 加锁
    Node<K,V>* current = this->head_node;   // 从头节点开始

    Node<K,V>* update[max_level + 1];       // 用于保存每一层中需要更新的节点
    memset(update, 0, sizeof(Node<K,V>*)*(max_level+1));  // 将数组初始化为0

    // 从最高层开始查找需要更新的节点
    for (int i = skip_list_level; i >= 0; i--) {
        while (current->forward[i] != NULL && current->forward[i]->get_key() < key) {
            current = current->forward[i];    // 更新节点
        }
        update[i] =current;    // 保存需要更新的节点
    }

    current = current->forward[0];  // 到达最底层并移动到下一个节点

    if (current != NULL && current->get_key() == key) {
        std::cout << "key: " << key << ", exists" << std::endl;
        mtx.unlock();   // 解锁
        return 1;
    }

    //如果current为NULL，则意味着我们已经到达该层的末尾
    //如果current键不等于key，这意味着我们必须在update[0]和当前节点之间插入节点
    //如上述例子中的40 和 60 之间
    if(current == NULL || current->get_key() != key){

        int random_level = get_random_level();  // 生成随机层数

        if(random_level > skip_list_level){    // 如果随机层数大于跳表的当前最高层数
            for (int i = skip_list_level+1; i < random_level+1; i++) {
                update[i] = head_node;  // 更新需要更新的节点
            }
            skip_list_level = random_level; // 更新跳表的层数
        }

        Node<K, V>* inserted_node = create_node(key, value, random_level); // 创建新节点

        for(int i = 0; i <= random_level; i++){
            inserted_node->forward[i] = update[i]->forward[i];  // 更新节点的指针
            update[i]->forward[i] = inserted_node;              // 更新需要更新的节点的指针
        }
        std::cout << "Successfully inserted key:" << key << ", value:" << value << std::endl;
        element_count++;    // 更新元素个数
    }
    mtx.unlock();   // 解锁
    return 0;
}

// 显示跳表
template<typename K, typename V>
void SkipList<K, V>::display_list() {

    std::cout << "\n*****Skip List*****"<<"\n";
    for (int i = 0; i <= skip_list_level; i++) {
        Node<K, V> *node = this->head_node->forward[i]; // 从头节点开始
        std::cout << "Level " << i << ": ";             // 打印层数
        while (node != NULL) {                          // 打印每一层的节点
            std::cout << node->get_key() << ":" << node->get_value() << "; ";
            node = node->forward[i];                    // 移动到下一个节点
        }
        std::cout << "\n";
    }
}

template<typename K, typename V>
void SkipList<K, V>::dump_file() {

    std::cout << "dump_file-----------------" << std::endl;
    _file_writer.open(STORE_FILE_PATH);             // 打开文件
    Node<K, V>* node = this->head_node->forward[0]; // 从头节点开始

    while (node != NULL) {                          // 写入每一层的节点
        _file_writer << node->get_key() << ":" << node->get_value() << "\n";
        std::cout << node->get_key() << ":" << node->get_value() << ";\n";
        node = node->forward[0];
    }

    _file_writer.flush();                        // 刷新缓冲区
    _file_writer.close();                        // 关闭文件
    return;
}

// 从文件中加载数据
template<typename K, typename V>
void SkipList<K, V>::load_file() {

    _file_reader.open(STORE_FILE_PATH);          // 打开文件
    std::cout << "load_file-----------------" << std::endl;
    std::string line;
    std::string key = new std::string();
    std::string value = new std::string();
    while (getline(_file_reader,line)){
        get_key_value_from_string(line, key, value);  // 从字符串中获取键值对
        if (key->empty() || value->empty()) {
            continue;
        }
        insert_element(*key, *value);                 // 插入元素
        std::cout << "key:" << *key << "value:" << *value << std::endl;
    }
    _file_reader.close();                             // 关闭文件
}

// 获取元素个数
template<typename K, typename V>
int SkipList<K, V>::size() {
    return element_count;
}

template<typename K, typename V>
bool SkipList<K, V>::is_valid_key(const std::string &key) {

    if(key.empty()){    // 如果key为空
        return false;
    }
    if(key.find(delimiter) == std::string::npos){   // 如果key中不包含分隔符 ":"
        return false;
    }
    return true;
}

template<typename K, typename V>
void SkipList<K, V>::get_key_value_from_string(const std::string &str, std::string *key, std::string *value) {

    if(!is_valid_key(str)){
        return;
    }
    *key = str.substr(0, str.find(delimiter));      // 获取key
    *value = str.substr(str.find(delimiter)+1);     // 获取value
}

// 删除元素
template<typename K, typename V>
void SkipList<K, V>::delete_element(K key) {

    mtx.lock(); // 加锁
    Node<K, V> *current = this->head_node;      // 从头节点开始
    Node<K, V> *update[max_level+1];            // 需要更新的节点
    memset(update, 0, sizeof(Node<K, V>*)*(max_level+1));

    // 从最高层开始，找到需要删除的节点
    for (int i = skip_list_level; i >= 0; i--) {
        while (current->forward[i] != NULL && current->forward[i]->get_key() < key) {
            current = current->forward[i];  // 移动到下一个节点
        }
        update[i] = current;    // 更新需要更新的节点
    }

    current = current->forward[0];
    //从最低层开始，删除每层的当前节点
    if (current != NULL && current->get_key() == key){
        for (int i = 0; i <= skip_list_level; i++) {
            //如果在第i层，下一个节点不是目标节点，则中断循环。
            if (update[i]->forward[i] != current) {
                break;
            }

            update[i]->forward[i] = current->forward[i];
        }

        // 更新跳表的层数,删除没有元素的层
        while (skip_list_level > 0 && head_node->forward[skip_list_level] == 0) {
            skip_list_level--;
        }

        std::cout << "Successfully deleted key "<< key << std::endl;
        element_count--;
    }
    mtx.unlock();   // 解锁
    return;
}


/*
                           +------------+
                           |  select 60 |
                           +------------+
level 4     +-->1+                                                      100
                 |
                 |
level 3         1+-------->10+------------------>50+           70       100
                                                   |
                                                   |
level 2         1          10         30         50|           70       100
                                                   |
                                                   |
level 1         1    4     10         30         50|           70       100
                                                   |
                                                   |
level 0         1    4   9 10         30   40    50+-->60      70       100
*/

// 查找元素
template<typename K, typename V>
bool SkipList<K, V>::search_element(K key) {

    std::cout << "search_element-----------------" << std::endl;
    Node<K, V> *current = this->head_node;      // 从头节点开始

    for (int i = skip_list_level; i >=  0; i--) {
        while (current->forward[i] != NULL && current->forward[i]->get_key() < key) {
            current = current->forward[i];  // 移动到下一个节点
        }
    }

    current = current->forward[0];  // 移动到下一个节点

    if (current and current->get_key() == key){
        std::cout << "Found key: " << key << ", value: " << current->get_value() << std::endl;
        return true;
    }
    std::cout << "Not Found Key:" << key << std::endl;
    return false;
}




