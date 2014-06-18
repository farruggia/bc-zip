/**
 * Copyright 2014 Andrea Farruggia
 * 
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 * 
 * 		http://www.apache.org/licenses/LICENSE-2.0
 * 
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
*/


/* AVLTree.h:
 * 
 * Two solutions for predecessor/successor queries: AVL-trees and VEB-like 
 * 
 */

#include <stdlib.h>
#include <algorithm>

#ifndef __AVLTREE__
#define __AVLTREE__

typedef int ktype; // the type of keys
typedef unsigned int UInt;

#define MAX_PATH 100

/*--------------------------------------------------------------
 Class AVLNode<T>: 
 Encapsulates an AVL-tree node, T is the type of satellite data
 ---------------------------------------------------------------*/

template<class T> class AVLNode{
 public:
  int height;         // height of the current node
  ktype key;          // node's key 
  T info;		      // additional data associated with current key	
  AVLNode<T>* left;   // pointer to left children
  AVLNode<T>* right;  // pointer to right children

  /* AVLNode(), AVLNode(key,info): nodes constructor */ 

  AVLNode(){
      left = right = NULL;
      height = 1;
  };
  AVLNode(ktype key, T info){
	this->key = key;
	this->info = info;
	left = right = NULL;
	height = 1;	
  };

  /*-----------------------------------------------------
   methods to perform clock-wise/ counterclock-wise rotation 
   of a node. parameter f is node's father
  ------------------------------------------------------*/

  inline void RRotate(AVLNode<T>* f){ 
	f->left = this->right;
	f->updateHeight();
	this->right = f;
	updateHeight();
  };
  
  inline void LRotate(AVLNode<T>* f){
	f->right = this->left;
	f->updateHeight();
	this->left = f;	
	updateHeight();
  };

  /* retrieve balancing of a node						*/

  inline int getBalance(){return Lheight()-Rheight();};

  /* update height of a node							*/
  
  inline void updateHeight(){ 
     int l = Lheight(), r = Rheight();
     height = std::max(l, r)+1;	
  };

  /* copy key and satellite data from node n		    */
  
  inline void Clone(AVLNode<T>* n) {this->key = n->key; this->info = n->info;};
  
  /* "detach" child n									*/
  
  inline void Unlink(AVLNode<T>* n) { 
      if (left == n) left = NULL; 
      else if (right == n) right = NULL;
   };
  
 private:
  /*------------------------------------------------------------------------------- 
  macros to compute the height of left/right child if it is not NULL or 0 otherwise 
  --------------------------------------------------------------------------------*/

  inline int Lheight() { return (left == NULL) ? 0: left->height;};
  inline int Rheight() { return (right == NULL) ? 0: right->height;}; 
};




/*--------------------------------------------------------------
 Class PathStack<T>: 
 A stack of pointers to AVLNode<T> objects. It is exploited by our
 AVL-tree implementation to keep track of all nodes accessed in an 
 insertion/deletion operation 
 ---------------------------------------------------------------*/

template<class T> class PathStack{
 public:
   AVLNode<T>* path[MAX_PATH];
   int sp;
  
   PathStack() {sp = 0;};
   inline void Push(AVLNode<T>* x) {path[sp++] = x;};
   inline AVLNode<T>* Top() {return path[sp-1];};
   inline AVLNode<T>* Pop() {return path[--sp];};
};



/*--------------------------------------------------------------
 Class AVLTree<T>: 
 An AVL-tree implementation. Nodes have keys of type ktype and T, the
 template parameter, is the type of satellite information.
 ---------------------------------------------------------------*/

template<class T> class AVLTree{
private:
 bool owns_memory;  // Do we own the nodes?
public:
 typedef AVLNode<T>* NodeRef;
 NodeRef root;						//pointer to root
 PathStack<T> path;				    //path accessed in the last operation

 AVLTree<T>(bool owns_memory = false): owns_memory(owns_memory), path() {root = NULL;};

 ~AVLTree<T>() {if (root && owns_memory) Release(root); };

 /* Insert(n): perform insertion of node n in the tree   */ 
 
 void Insert(NodeRef n){

     ktype k = n->key; 
	 if (root == NULL) root = n;
	 else{
		NodeRef x=root,p=root;	
  		while (x != NULL) {
  		   path.Push(x);
  		   x=(k <= x->key )? x->left: x->right; 	
  		}
	   p = path.Top();
  	   if (k <= p->key) p->left = n;
  	   else p->right = n;
       GlobalRebalance();
     }	
 };

 // void Insert(ktype k, T info) {Insert(new AVLNode<T>(k,info));};

 /* 
  Delete(key): delete (one of) the node with the specified key from the tree
  and return a pointer to it
  */

 NodeRef Delete(ktype key){
 
  NodeRef x = root, y;
  NodeRef res = NULL;
  AVLNode<T> tmp;

  while (x != NULL && x->key != key){  
      path.Push(x);
	  x = (key < x->key) ? x->left : x->right;
   }

  if (x != NULL){ 
  if (x->height == 1) {
	 if (x == root) root = NULL; 
	 else { path.Top()->Unlink(x); 
		    GlobalRebalance(); }
	 return x;
  } else {
	  tmp.Clone(x);
	  path.Push(x);
	  if (x->left == NULL) {
          x->Clone(x->right);
		  res = x->right;
		  x->Unlink(x->right);
	  } else {
	     y = x->left;
		 while (y->right != NULL) {
				path.Push(y);
  				y=y->right; 	
		 }
         if (y->left == NULL) {
			    x->Clone(y);
				res = y;
				path.Top()->Unlink(y);
		  } else {
			    path.Push(y);
				x->Clone(y);  
				y->Clone(y->left);
				res = y->left;
				y->Unlink(y->left);
		  }
	}
  }
  res->Clone(&tmp);
  }
  
  GlobalRebalance();
  return res;
};

 /*
  Search(key, *prev, *next): search for the two nodes in the tree whose keys are respectively the immediate 
  predecessor and successor of key. At the end prev and next will store pointers to these two nodes
 */
 void Search(ktype key, AVLNode<T>** prev, AVLNode<T>** next){
  *prev = *next = NULL;
  NodeRef x = root;

  while (x != NULL){
	if (x->key == key) {*prev= *next = x; return;}
	else if (x->key < key) { *prev = x; x = x->right;}
	else {*next = x; x = x->left;}
   }
 };

 // return TREE iff the tree is empty
 inline bool isEmpty() {return (root == NULL);};
 
private:

 /* LocalRebalance(point): perform rebalancing of a single node in the tree. 
    point is the node to be rebalanced. The return value is a pointer to the new root
	of the subtree rooted at point*/

 inline NodeRef LocalRebalance(NodeRef point){

	point->updateHeight();
	NodeRef nroot = point;

	if (point->getBalance() < -1){
		NodeRef r = nroot = point->right;	 
		if (r->getBalance() > 0) { 
   		    nroot = r->left; 	
   			nroot->RRotate(r);
		}	
	   nroot->LRotate(point); 
	 }	
	else if (point->getBalance() > 1){
		NodeRef l = nroot = point->left;	 
		if (l->getBalance() < 0) { 
   			nroot = l->right; 	
   			nroot->LRotate(l);
		}	
		nroot->RRotate(point); 
	} 
  
 return nroot; 	
};

/* GlobalRebalance(): apply rebalancing to every node in PathStack*/

void GlobalRebalance(){
	
	NodeRef f,n=path.Pop();
    NodeRef* q;

	while(path.sp > 0) {
	  f=path.Pop();
	  q = (f->left == n) ? &(f->left): &(f->right);
	  *q = LocalRebalance(n);
	  n=f;
	} 
   root = LocalRebalance(n);
};

/* Release(n): delete any descendant of node n (called only by destructor) */

void Release(NodeRef n){
	if (n != NULL) {
	  Release(n->left);
	  Release(n->right);
	  delete n;
	}
 };

};

/*----------------------------------------------------------------------------
 
  BMUtil: utility functions for previous/next bit set in a 32-bits memory word 
 
 ----------------------------------------------------------------------------*/


class BMutil{	
public:

static int first[256];
static int revfirst[256];

static void FillInTables(){
	int x;
	first[0] = revfirst[0] = -1;

	for (int j=1; j<256; j++)
	{
      first[j] = 0; 
	  revfirst[j] = 7; 
	  x = j;
	  while ( !(x % 2) ) {first[j]++; x >>= 1;}
	  x = j;
	  while (x != 1) {revfirst[j]--; x >>= 1;}
	}
};

static inline int minBit(UInt x){
   if (!x) return -1;
   int off = 0;
   while (!(x & 255)) {x >>= 8; off+=8;}
   return first[x & 255] + off; 
};

static inline int maxBit(UInt x){
  if (!x) return -1;
  int off = 31;
  const UInt lastbyte = 255 << 24;
  while (!(x & lastbyte)){ x <<=8; off-=8; }
  return off-revfirst[(x&lastbyte)>>24];
};

static inline int Succ(UInt b, unsigned int p){
	UInt x = (b >> p) >> 1;
	int off;
	if (!x) return -1;
	off = p+1;
	while (!(x & 255)) {x >>= 8; off+=8;}
	return first[x & 255] + off; 
	
};

static inline int Pred(UInt b, int p){
	UInt x = (b << (31 - p)) << 1;
	int off;
	const UInt lastbyte = 255 << 24;
	if (!x) return -1;
	off = p-1;
	while (!(x & lastbyte)) {x <<= 8; off -= 8; }  
	return off - revfirst[(x>>24) & 255];
};

static inline int Log32(UInt x){

 UInt y = x;
 int res=0;
 
 while (y > 1) {
 	 y = (y+31)/32; 
 	 res++;
 }
 
 return res;

};

};

/*---------------------------------------------------------------------------
 
  PSStruct: Van-Emde Boas-like Data Structure for answering pred./succ. query  
  
 ---------------------------------------------------------------------------*/


class PSStruct: public BMutil{
 public:	
	UInt **bm;
	UInt **min;
	UInt **max;
	int height;
	//UInt *treesize;
	
	PSStruct(){bm = min = max = NULL; height = 0;};
	PSStruct(int range){SetRange(range); };
	
	~PSStruct(){

	 if (height) {
	  delete[] bm[0];	
	  for (int j=1; j < height; j++){
	   delete[] bm[j];
	   delete[] min[j];
	   delete[] max[j]; 	
	  }
	  delete[] min;
	  delete[] max;
	  delete[] bm;
	 }
	};
	
	inline void SetRange(int range) {
		 height = Log32(range);
		 bm= new UInt*[height];
		 min= new UInt*[height];
		 max= new UInt*[height];
		 int tmp= range+1;
		 int j= 0;

		 while (tmp > 1){
		  tmp = (tmp+31)/32;
		  bm[j] = new UInt[tmp];
		  for (int i=0; i < tmp; i++) bm[j][i] = 0;
		  if (j) {
			  min[j] = new UInt[tmp];
			  max[j] = new UInt[tmp];
			  for (int i=0; i < tmp; i++) {min[j][i] = 1 << 31; max[j][i] = 0;}
		  }
		  else {min[j] = max[j] = NULL;}
		  j++;
		 }
	}

	inline void Set(UInt elem) {

		 int j = 1, off, ind;

		 // Set the corresponding bit at the base-level bitmap
		 off = (elem) >> 5;
		 ind = elem & 31;
		 bm[0][off] |= 1 << ind;

		 // Walk-up the tree updating the structure

		 while (/*toUp && bm[j] != NULL*/ j < height){

		   // determine offset and index at the current level
		   ind = off & 31;
		   off >>= 5;

		   if (!bm[j][off]) // if the word is empty
		     {
		      min[j][off] = max[j][off] = elem;   // set the current minimum and maximum to elem
		      bm[j][off]  |= 1 << ind;
		      }
		   else{
		   	 bm[j][off] |= 1 << ind;
		   	 if (min[j][off] > elem) {min[j][off] = elem;}
		   	 else if (max[j][off] < elem) {max[j][off] = elem;}
		   	 else return; // if neither min and max get changed stop
		   }

		   j++;
		 }
	}
	inline void Remove(UInt elem) {

		 int j=0, off = elem, ind;
		 int val;

		 do {
		  ind = off & 31;
		  off = off >> 5;
		  bm[j][off] ^= 1 << ind;
		 } while (!bm[j][off] && ++j < height);

		 if (j < height){

		     UInt bitind = (1 << ind) - 1;
		    // Recompute maximum and minimum of the current node
			 if (!(bm[j][off] & bitind)){// recompute the minimum if it is changed
			    if (!j) val = findMin(off,0);
				else {
		           val =  findMin((off<<5) + minBit(bm[j][off]),j-1);
				   min[j][off]= val;
				}
			 } else if (!(bm[j][off] & ~bitind)){// recompute the maximum if it is changed
		        if (!j) val = findMax(off,0);
				else {
		           val = findMax((off<<5) + maxBit(bm[j][off]),j-1);
				   max[j][off] = val;
				}
			 } else return;// if max and min don't change terminate

			 while (++j < height){
				ind = off & 31;
				off = off >> 5;
				if (min[j][off] == elem) min[j][off]= val;
				else if (max[j][off] == elem) max[j][off] = val;
				else return;
			 }
		   }
	}
	inline void Search(UInt elem, int &p, int &s) {

		 int off = elem >> 5, ind = elem & 31;
		 int lev = 0, toUp = 0;
		 p = s = -1;
		 int tmp;

		  while (toUp != 2 && /*bm[lev] != NULL*/ lev < height){

		   if (bm[lev][off]){
			 if (p < 0 && (tmp = Pred(bm[lev][off],ind)) >= 0) {
				 p = (lev) ? findMax(tmp + (off << 5), lev-1) : tmp + (off << 5);
				 toUp ++;
			 }

			 if (s < 0 && (tmp = Succ(bm[lev][off],ind)) >= 0) {
				 s = (lev) ? findMin(tmp + (off << 5), lev-1): tmp + (off << 5);
				 toUp++;
		 	 }
		    }
		     ind = off & 31;
			 off >>= 5;
		     lev++;

		  }
	}

 private: 
    
    inline int findMin(int off, int lev) {
    	if (lev) return min[lev][off];
    	else return (minBit(bm[lev][off]) + (off << 5));
    }

    inline int findMax(int off, int lev) {
    	if (lev) return max[lev][off];
    	else return (maxBit(bm[lev][off]) + (off << 5));
    }
	
};


#endif
