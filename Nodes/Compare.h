//
//  Compare.h
//  Nodes
//
//  decides order in which operations are dealt with
//  in priority queue

class Compare
{
public:
    bool operator() (const Operation* op1, const Operation* op2) const
    {
        if (op1->seq_num >= op2->seq_num) return true;
        else return false;
    }
};