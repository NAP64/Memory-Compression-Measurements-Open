/*

    A simple Huffman-encoding generating code
    For page-level hardware memory compression.
    This implementation focuses on speed.

    This implementation only consider bytes and uses canonical Huffman code
    Compressed data stores depth-element count and a dictionary. 
    A escape character and a plain text is used for elements that has occurence lower than a limit.

    By Yuqing Liu
    HEAP Lab, Virginia Tech 
    Apr 2019
*/

#include <stdint.h>
#include <stdio.h>

#include <BitStream64.h>

#define Huff_if_tree(a) (a >= 257)
#define Huff_tree2index(a) (a >= 257 ? a - 257 : a)
#define Huff_make_tree(a) (a + 257)
#define LOW_OCC_LIMIT(a) (9) //((int)(8.0/4096*a)), magic number got by trial&error, may fail depend on usage

struct linked_node      //For element occurence and tree depth. May be repurposed
{
    int16_t info;       //Count, depth, element, etc
    int16_t next;       //offset of next node in array, -1 for end
};

struct linked_tree_node //Similar to linked node. added tree offsets. May be repurposed
{
    int16_t info;
    int16_t next;
    int16_t left;
    int16_t right;
};

/*
 * Sort a linked list of byte_node. next = -1 is end. 
 * *List must be constructed before use
 * takes parameter of memory block of byte_node, and current head of list
 * returns head of list and tail of list in parameter
 */
static int16_t Huffman1_quick_sort(struct linked_node * list, int16_t head, int16_t * tail)
{
    int16_t heads[2], tails[2], i;
    heads[0] = heads[1] = tails[0] = tails[1] = -1;
    *tail = head;
    //construct the two lists
    for (i = list[head].next; i != -1; i = list[i].next)
    {
        int temp = 1;
        if (list[i].info < list[head].info)
            temp = 0;
        if (heads[temp] == -1)
            heads[temp] = i;
        else
            list[tails[temp]].next = i;
        tails[temp] = i;
    }
    //sort and attach lists together
    list[head].next = -1;
    if (tails[0] != -1)
    {
        list[tails[0]].next = -1;
        heads[0] = Huffman1_quick_sort(list, heads[0], &(tails[0]));
        list[tails[0]].next = head;
        head = heads[0];
    }
    if (tails[1] != -1)
    {
        list[tails[1]].next = -1;
        heads[1] = Huffman1_quick_sort(list, heads[1], &(tails[1]));
        list[*tail].next = heads[1];
        *tail = tails[1];
    }
    return head;
}

//write depth to tree nodes, recursivly
static void Huff_tree_level(struct linked_tree_node* tree, struct linked_node* nodes, int16_t root, int16_t level)
{
    if (Huff_if_tree(root))
    {
        root = Huff_tree2index(root);
        Huff_tree_level(tree, nodes, tree[root].left, level + 1);
        Huff_tree_level(tree, nodes, tree[root].right, level + 1);
    }
    else
        nodes[root].info = level;
}

//Normalizes a linked list of leaf nodes to depth <= 15, recursivly
//list should be sorted from deeper to shallower, or, by less to more occurrences
//valid depth should be already writen in count field
//uses table to speed up process. idealy O(n) for entire tree. 
static int16_t Huff_normalize_tree(struct linked_node* nodes, int16_t head, int16_t count)
{
    static int16_t numList[16] = {0x7fff, 0x3fff, 0x1fff, 0xfff, 0x7ff, 0x3ff, 0x1ff, 0xff, 0x7f, 0x3f, 0x1f, 0xf, 0x7, 0x3, 0x1, 0x0};
    static uint8_t numTable1[127] = {   0x0, 
                                        0x1,    0x1,    0x3,    0x3,    0x3,    0x3,    0x7,    0x7, 
                                        0x7,    0x7,    0x7,    0x7,    0x7,    0x7,    0xf,    0xf,
                                        0xf,    0xf,    0xf,    0xf,    0xf,    0xf,    0xf,    0xf,
                                        0xf,    0xf,    0xf,    0xf,    0xf,    0xf,    0x1f,   0x1f,
                                        0x1f,   0x1f,   0x1f,   0x1f,   0x1f,   0x1f,   0x1f,   0x1f,
                                        0x1f,   0x1f,   0x1f,   0x1f,   0x1f,   0x1f,   0x1f,   0x1f,
                                        0x1f,   0x1f,   0x1f,   0x1f,   0x1f,   0x1f,   0x1f,   0x1f,
                                        0x1f,   0x1f,   0x1f,   0x1f,   0x1f,   0x1f,   0x3f,   0x3f,
                                        0x3f,   0x3f,   0x3f,   0x3f,   0x3f,   0x3f,   0x3f,   0x3f,
                                        0x3f,   0x3f,   0x3f,   0x3f,   0x3f,   0x3f,   0x3f,   0x3f,
                                        0x3f,   0x3f,   0x3f,   0x3f,   0x3f,   0x3f,   0x3f,   0x3f,
                                        0x3f,   0x3f,   0x3f,   0x3f,   0x3f,   0x3f,   0x3f,   0x3f,
                                        0x3f,   0x3f,   0x3f,   0x3f,   0x3f,   0x3f,   0x3f,   0x3f,
                                        0x3f,   0x3f,   0x3f,   0x3f,   0x3f,   0x3f,   0x3f,   0x3f,
                                        0x3f,   0x3f,   0x3f,   0x3f,   0x3f,   0x3f,   0x3f,   0x3f,
                                        0x3f,   0x3f,   0x3f,   0x3f,   0x3f,   0x3f};
    static uint8_t numTable2[127] = {   15,
                                        14, 14, 13, 13, 13, 13, 12, 12,
                                        12, 12, 12, 12, 12, 12, 11, 11,
                                        11, 11, 11, 11, 11, 11, 11, 11,
                                        11, 11, 11, 11, 11, 11, 10, 10,
                                        10, 10, 10, 10, 10, 10, 10, 10,
                                        10, 10, 10, 10, 10, 10, 10, 10,
                                        10, 10, 10, 10, 10, 10, 10, 10,
                                        10, 10, 10, 10, 10, 10, 9,  9,
                                        9,  9,  9,  9,  9,  9,  9,  9,
                                        9,  9,  9,  9,  9,  9,  9,  9,
                                        9,  9,  9,  9,  9,  9,  9,  9,
                                        9,  9,  9,  9,  9,  9,  9,  9,
                                        9,  9,  9,  9,  9,  9,  9,  9,
                                        9,  9,  9,  9,  9,  9,  9,  9,
                                        9,  9,  9,  9,  9,  9,  9,  9,
                                        9,  9,  9,  9,  9,  9};
    int16_t lev = 0;
    if (nodes[head].info > 15) //normalize larger depth to 15
    {
        count = count + 1;
        nodes[head].info = 15;
    }
    else if (count == 0)
        return 0;
    else    //if need more space in the depth 15 row
    {
        count = count - numList[nodes[head].info]; //reduce space needed
        nodes[head].info = 15;
    }
    if (count > 0)  //need more space from next node
        lev = Huff_normalize_tree(nodes, nodes[head].next, count);
    else    //got enough or extra 
        lev = -count;
    if (lev > 0) //reduce depth to ultilizes left over spaces.
    {
        if (lev < 127)
        {
            nodes[head].info = numTable2[lev];
            lev -= numTable1[lev];
        }
        else if (lev < 255)
        {
            nodes[head].info = 8;
            lev -= 127;
        }
        else if (lev < 511)
        {
            nodes[head].info = 7;
            lev -= 255;
        }
        else if (lev < 1023)
        {
            nodes[head].info = 6;
            lev -= 512;
        }
        else if (lev < 2047)
        {
            nodes[head].info = 5;
            lev -= 1023;
        }
        else
        {
            nodes[head].info = 4;
            lev -= 2047;
        }
    }
    return lev;
}

//Dest should be at least size + 12 + 256 bytes long to hold all data.
//Returns compressed size in bytes
static uint64_t Huffman1_encode(uint8_t * data, uint8_t * dest, int size)
{
    //initialize data
    struct linked_node literals[257];
    int i, cur;
    for (i = 0; i < 257; i++)
        literals[i].info = 0;
    
    //find occurance of bytes
    for (i = 0; i < size; i++)
        literals[data[i]].info++;

    //create list for sort and ignore byte that didn't appear
    //find first non-zero 
    for (cur = 0; cur < 257; cur++)
        if (literals[cur].info >= LOW_OCC_LIMIT(size))
            break;
        else
        {
            literals[256].info += literals[cur].info;
            literals[cur].info = 0;
        }

    literals[cur].next = -1; //Use as list end
    int16_t prev = cur;
    int16_t head = cur;
    //form a list
    for (cur++; cur < 257; cur++)
    {
        if (literals[cur].info >= LOW_OCC_LIMIT(size) || (cur == 256 && literals[cur].info > 0))
        {
            literals[prev].next = cur;
            prev = cur;
        }
        else
        {
            literals[256].info += literals[cur].info;
            literals[cur].info = 0;
        }
    }
    literals[prev].next = -1;

    //short-circuit logic for incomplete tree 
    //would be 1 node at depth 0 but not allowed by storage structure
    if (literals[prev].info == size)
    {
        if (prev != 256)
        {
            dest[0] = 0x00;
            dest[1] = prev & 0xff;
            return 2;
        }
        else
        {
            dest[0] = 0x01;
            for (i = 0; i < size; i++)
                dest[i + 1] = data[i];
            return size + 1;
        }
    }
    //No we have at least 2 elements in tree, depth 1 or more

    //sort the list
    head = Huffman1_quick_sort(literals, head, &prev);

    //use tree nodes to generate huffman tree
    struct linked_tree_node tree [256];
    cur = head;
    int16_t cur_tree = -1;
    int16_t tree_index = 0;
    //each time generate one new tree node until one node left
    while (cur != -1 || tree[cur_tree].next != -1)
    {
        tree[tree_index].info = 0;
        int16_t offsprings[2];
        for (i = 0; i < 2; i++)
        {
            //get node from tree
            if ((cur_tree != -1) && (cur == -1 || tree[cur_tree].info < literals[cur].info))
            {
                tree[tree_index].info += tree[cur_tree].info;
                offsprings[i] = Huff_make_tree(cur_tree);
                cur_tree = tree[cur_tree].next;
            }
            //get node from literals
            else
            {
                tree[tree_index].info += literals[cur].info;
                offsprings[i] = cur;
                cur = literals[cur].next;
            }
        }
        tree[tree_index].left = offsprings[0];
        tree[tree_index].right = offsprings[1];
        if (cur_tree == -1)
        {
            cur_tree = tree_index;
            tree[tree_index].next = -1;
        }
        else if (tree[tree_index - 1].next == -1)
        {
            tree[tree_index - 1].next = tree_index;
            tree[tree_index].next = -1;
        }
        else    //impossible fall-back condition with old code
        {
            printf("?\n");
            int16_t temp = -1;
            for (   prev = cur_tree; 
                    tree[prev].info <= tree[tree_index].info; 
                    prev = tree[prev].next)
                temp = prev;
            if (temp == -1)
            {
                tree[tree_index].next = cur_tree;
                cur_tree = tree_index;
            }
            else
            {
                prev = tree[temp].next;
                tree[temp].next = tree_index;
                tree[tree_index].next = prev;
            }
        }
        tree_index++;
    }
    
    //Convert occurrence to depth in tree
    Huff_tree_level(tree, literals, Huff_make_tree(cur_tree), 0);
    
    //Now we can discard the tree and use a table instead.
    //length of codeword is count
    //But first, normalize to max length of 15
    Huff_normalize_tree(literals, head, 0);

    //codeword generation
    struct linked_node table[16];
    for (i = 0; i < 16; i++)
    {
        table[i].info = 0;
        table[i].next = -1;
    }
    prev = cur = literals[head].info; //record largest depth
    for (i = 256; i >=0 ; i--)
    {
        if (!!literals[i].info && literals[i].info < cur)
            cur = literals[i].info;
        int16_t next = table[literals[i].info].next;
        table[literals[i].info].next = i;
        table[literals[i].info].info ++;
        literals[i].next = next;
    }

    //write dictionary
    dest[0] = (literals[256].info & 0xf) | ((prev & 0xf) << 4); //record tree depth and escape char depth
    

    if (prev == cur) //all literals in one level
    {
        dest[1] = 0x80;
        cur = 2;
    }
    else
    {
        //depth 1:1 2:2 3:3 4:4 5:5 6:6 in 3 bytes: 1:1:5 2:6 3:4
        //These three bytes will always be used.
        dest[1] = ((table[1].info & 1) << 5) | (table[5].info & 0x1f);
        dest[2] = ((table[2].info & 3) << 6) | (table[6].info & 0x3f);
        dest[3] = ((table[3].info & 7) << 4) | (table[4].info & 0xf);
        //write the following depth counts
        for (cur = 4; cur <= prev - 3; cur++)
            dest[cur] = 0xff & table[cur + 3].info;
    }
    
    //generate codeword for each entry and write plain text for dict
    int16_t cur_depth, next, codeword = 0;
    for (i = 1; i < 16; i++)
    {
        //printf("%d,", table[i].info);
        cur_depth = table[i].next;
        while (cur_depth != -1)
        {
            next = literals[cur_depth].next;
            literals[cur_depth].next = codeword;
            codeword++;
            if (cur_depth != 256)
            {
                dest[cur] = cur_depth & 0xff;
                cur++;
            }
            cur_depth = next;
        }
        codeword = codeword << 1;
    }
    
    //printf("%x: new\n", cur);

    //encode data to dest
    BitStream64_t out;
    BitStream64_write_init(&out, dest + cur);
    for (i = 0; i < size; i++)
    {
        //printf("%x ", literals[data[i]].info);
        if (!literals[data[i]].info)
        {
            BitStream64_write(&out, (uint16_t)literals[256].next, literals[256].info);          //write escape codeword
            BitStream64_write(&out, data[i], 8);                                                //write literal
        }
        else
            BitStream64_write(&out, (uint16_t)literals[data[i]].next, literals[data[i]].info);  //write huffman codeword
    }
    cur += BitStream64_write_finish(&out);
    //printf("\n");
    return cur; 
}

//dest should be at least size bytes long to hold all data.
//size is decode size
//returns compressed size
static uint64_t Huffman1_decode(uint8_t * data, uint8_t * dest, int size)
{
    int i, j, cur;
    if (!data[0])
    {
        for (i = 0; i < size; i++)
            dest[i] = data[1];
        return 2;
    }
    if (data[0] == 1)
    {
        for (i = 0; i < size; i++)
            dest[i] = data[i + 1];
        return i + 1;
    }

    //retrive dict
    int16_t depth = (data[0] >> 4) & 0xf;
    int16_t escape = data[0] & 0xf;

    //initialize table
    struct linked_tree_node table[16];
    for (i = 0; i < 16; i++)
    {
        table[i].info = 0;
        table[i].next = -1;
        table[i].left = -1;
        table[i].right = -1;
    }
    if (!(data[1] & 0x80))
    {
        table[1].info = (data[1] >> 5) & 1;
        table[5].info = data[1] & 0x1f;
        table[2].info = (data[2] >> 6) & 3;
        table[6].info = data[2] & 0x3f;
        table[3].info = (data[3] >> 4) & 7;
        table[4].info = data[3] & 0xf;
        cur = 4;
        for (cur = 4; cur <= depth - 3; cur++)
            table[cur + 3].info = data[cur];
    }
    else // all on one depth
    {
        table[depth].info = 1 << depth;
        cur = 2;
    }
    
    
    //generate minimum codeword for each head of table
    //generate lookup for faster decode
    int16_t codeword = 0;
    for (i = 1; i < 16; i++)
    {
        //printf("%x\n", table[i].info);
        if (table[i].info > 0)
        {
            table[i].left = codeword;
            codeword += table[i].info;
            table[i].right = cur;       //offset into dictionary
            if (i == escape)            //reduce one for escape char, not in dict
                cur--;
            cur += table[i].info;
        }
        codeword = codeword  << 1;
    }

    //printf("%x\n", cur);

    //mask for literal constrction
    static uint8_t mask[] = {0, 1, 3, 7, 0xf, 0x1f, 0x3f, 0x7f, 0xff};
    //recover original data
    uint8_t payload = data[cur];
    cur++;
    uint8_t offset = 0;
    for (i = 0; i < size; i++)
    {
        codeword = 0;
        for (j = 1; j < 16; j++)
        {
            offset++;
            if (offset == 9) //next byte
            {
                payload = data[cur];
                cur++;
                offset = 1;
            }
            codeword = (codeword << 1) | (uint16_t)(1 & ((uint16_t)payload >> (8 - offset)));   //construct codeword
            if (codeword >= table[j].left && table[j].info > 0 && codeword < table[j].info + table[j].left)
            {
                //printf("%x ", j);
                if (j == escape && codeword == table[j].info + table[j].left - 1) //matched escape char
                {
                    if (offset == 8)
                        dest[i] = data[cur];
                    else
                    {
                        uint8_t temp = payload << offset;
                        payload = data[cur];
                        dest[i] = temp | ((payload >> (8 - offset)) & mask[offset]);
                    }
                    cur++;
                }
                else
                    dest[i] = data[codeword - table[j].left + table[j].right];
                break;
            }
        }
    }
    //printf("\n");
    return cur;
}

