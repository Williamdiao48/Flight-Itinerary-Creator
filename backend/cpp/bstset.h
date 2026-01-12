//
//  bstset.h
//  CS32 Project 4 Air Anarchy
//
//  Created by William Diao on 3/17/25.
//

//Your BSTSet<T> class must not use any STL containers.
//● Your class must NOT have any public data members and must NOT have any public
//methods or public nested structs/classes other than those mentioned. Private members
//and structs/classes are allowed, of course.
#ifndef BSTSET_H
#define BSTSET_H
#include <stack>
#include <iostream>

template <typename T>
class BSTSet{
private:
    //Root of BST tree
    struct Node{
        T data;
        Node* right;
        Node* left;
        Node* parent;
    Node(const T&val, Node*p = nullptr):data(val), right(nullptr), left(nullptr), parent(p){}
    };
    Node* root; //root of BST tree

    void insertHelper(Node*& node, Node* parent, const T& value){
        if(node==nullptr){
            node = new Node(value, parent);
        }
        else if (value<node->data){
            insertHelper(node->left, node, value); //Recursively insert into left subtree
        }
        else if (node->data<value){
            insertHelper(node->right, node, value); //Recursively insert into right subtree
        }
        else{
            node->data = value; //Since the value is already present, replace the existing value with this value
        }
    }
    
    Node* findHelper(Node* node, const T& value) const{
        if (node==nullptr){
            return node;
        }

        if (value < node->data) {
            return findHelper(node->left, value); //Recursively search into left subtree
        }
        else if (value > node->data) {
            return findHelper(node->right, value); //Recursively search into right subtree
        }
        else {
            return node;  // Found the node
        }
    }
    
    
    Node* findFirstNotSmallerHelper(Node* node, const T& value) const{
        if (node==nullptr){
            return nullptr;
        }
        if(value<node->data){
            Node* leftResult = findFirstNotSmallerHelper(node->left, value); //Check if any better candidates in left subtree
            if (leftResult) {
                return leftResult;  // Found a smaller valid node in the left subtree
            }
            return node; //This is best candidate
        }
        return findFirstNotSmallerHelper(node->right, value); //Search right subtree for greater values
    }
    
    void destroyTree(Node* node){
        if (node != nullptr) {
            destroyTree(node->left);
            destroyTree(node->right);
            delete node;
        }
    }
    
public:
    BSTSet() : root(nullptr){}  // Constructor
    ~BSTSet(){destroyTree(root);}
    
    class SetIterator{
    private:
        Node* current;
        
        Node* getNext(Node* node) {
            if (node == nullptr) return nullptr;

            //If right child exists go to the right subtree and find leftmost node
            if (node->right != nullptr) {
                node = node->right;
                while (node->left != nullptr) {
                    node = node->left;
                }
                return node;
            }

            // Otherwise move up tree using parent pointers
            while (node->parent != nullptr && node == node->parent->right) {
                node = node->parent;
            }

            // Return the parent of the current node or nullptr if no valid parent exists
            return node->parent;
        }


    public:
        SetIterator(Node* node) : current(node) {}
        
        const T* get_and_advance(){ //Returns current element and advances
            if (current == nullptr) return nullptr;

            //Save current node data and move to next element
            const T* data = &current->data;

            // Advance the iterator to next node
            current = getNext(current);
            return data;
        }
    };
    
    
    void insert(const T& value){
        insertHelper(root, nullptr, value); //calls helper function
    }
    
    SetIterator find(const T& value) const{
        Node* node = findHelper(root, value); //calls helper function
        return SetIterator(node);
    }
    
    SetIterator find_first_not_smaller(const T& value) const{
        Node* node = findFirstNotSmallerHelper(root, value);
        if (node == nullptr) {
            return SetIterator(nullptr);  // Return invalid iterator if no such element is found.
        }
        return SetIterator(node);  // Return the iterator pointing to the found node.
    }
    
};


#endif
