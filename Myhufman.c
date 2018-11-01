#include <stdlib.h>
#include <stdio.h>


/*____________________________________TYPE________________________________________________________________*/

typedef struct _symb {
	unsigned char ch;
	float freq;
	short isComposition;//calloc inits 0
}symb;

typedef symb* freqTable;

typedef struct { unsigned short b : 1; } bit;

typedef union _byte {
	unsigned char val;
	bit b[CHAR_BIT];
}byte;

typedef struct _hufNode {
	symb symbol;
	struct _hufNode *left;
	struct _hufNode *right;
}hufNode;

typedef struct _hufTableCell{
	unsigned char ch;
	unsigned char HufCodeLen;
	bit code[UCHAR_MAX]; //?количество битов кода однозначно меньше битмах так как оно будет меньше log2(255) по свойствам бинарного дерева
}hufTableCell;

typedef struct _hufTable {
	unsigned char len;
	hufTableCell table[UCHAR_MAX];
}hufTable;

/*____________________________________Const________________________________________________________________*/

const bit b0 = { 0 };
const bit b1 = { 1 };
/*____________________________________Var________________________________________________________________*/

unsigned char buf[BUFSIZ];

hufTable HTable;

/*_________________________________Functions____________________________________________________*/

void FTadd(freqTable *adrFT,symb *pnewSymb,unsigned char *puniqchrs,char placeInSorted) {
	if ((pnewSymb != NULL) && (adrFT != NULL))
	{//вставлять в нужное место
		if (*adrFT == NULL) *adrFT = (symb *)calloc(1, sizeof(symb));
		*adrFT = (freqTable)realloc(*adrFT, ++(*puniqchrs) * sizeof(symb));
		char placeToInsert = (*puniqchrs) - 1;
		if (placeInSorted) {
			while ((placeToInsert > 0) && ((*adrFT)[placeToInsert - 1].freq < pnewSymb->freq)) 
				placeToInsert--;//таблица отсортирована по убыванию
			//placeToInsert++;//поправка на последнюю проверку, которая не была пройдена, но значение placeToInsert уменьшено 
			for (char i = (*puniqchrs) - 1; i > placeToInsert; i--)
				(*adrFT)[i] = (*adrFT)[i - 1];//большие траты на перекидование структур символов а не их указателей
			(*adrFT)[placeToInsert] = *pnewSymb;
		}
		else
			(*adrFT)[placeToInsert] = *pnewSymb;
	}
}

void FTDelLastEl(freqTable *adrFT, unsigned char *puniqchrs) {
	//free((*adrFT)[*puniqchrs]); если будешь менять символы на указатель символов
	*adrFT =(freqTable)realloc(*adrFT,--(*puniqchrs) * sizeof(symb));//надо ли?
}

int cmpFreq(symb *const pa,symb *const pb) {
	return (pb->freq - pa->freq) < 0 ? -1 : 1;
}

freqTable *dofreqTable(FILE *f, unsigned char *puniqchrs) {
	/*expects f to be open "rb"
		uniqchrs = len(FT)	*/

	unsigned long numCh[UCHAR_MAX + 1] = { 0UL };
	unsigned long prevfPos = ftell(f);
	unsigned long flen = 0;
	unsigned long bytesRead = 0;
	do 
	{
		bytesRead = fread_s(buf, sizeof(unsigned char) * BUFSIZ,sizeof(unsigned char), BUFSIZ, f);
		for (unsigned long i = 0; i < bytesRead; i++)
		{
			numCh[buf[i]]++;//counts number of meeting each byte in f
			flen++;
		}
	} while (bytesRead == BUFSIZ);
	static freqTable FT;
	symb NewSymb;
	*puniqchrs = 0;
	for (int i = 0; i < UCHAR_MAX + 1; i++)
		if (numCh[i] != 0) {
			NewSymb.ch = i;
			NewSymb.freq = (float)numCh[i] / flen;
			NewSymb.isComposition = 0;
			FTadd(&FT, &NewSymb, puniqchrs, 0);
		}
	fseek(f, prevfPos, SEEK_SET);
	qsort(FT, *puniqchrs, sizeof(symb), cmpFreq);
	return &FT;
}

hufNode *doHufNode(symb *pnewSymb) {
	hufNode *res = (hufNode *)calloc(1, sizeof(hufNode));
	res->symbol = *pnewSymb; 
	return res;
};

hufNode *doHufTree(freqTable *adrFT,unsigned char uniqchrs) {
	#define MAXlenARRlinked  128 //max number of compositional nodes that have no parent
	hufNode *leftchild = NULL, *rightchild = NULL, *unlNodes[MAXlenARRlinked] = {NULL}, *result = NULL;
	unlNodes[0] = NULL; unlNodes[1] = NULL;
	//влад
	static symb resSymb;
	const nodesComposing = 2;
	unsigned char i = uniqchrs - 1;
	while (i > 0)
	{
		if ((*adrFT)[i].isComposition) {
			for (unsigned char j = 0; j < MAXlenARRlinked; j++)
			if ((unlNodes[j] != NULL) && (unlNodes[j]->symbol.freq == (*adrFT)[i].freq)) { //dirty comparison
				rightchild = unlNodes[j];
				//free(unlNodes[0]); ты о чем вообще ты структуру строишь так то
				unlNodes[j] = NULL;
				break;
			}
		}else
			rightchild = doHufNode((*adrFT) + i);
		if ((*adrFT)[i - 1].isComposition) {
			for (unsigned char j = 0; j < MAXlenARRlinked; j++)
				if ((unlNodes[j] != NULL) && (unlNodes[j]->symbol.freq == (*adrFT)[i - 1].freq)) { //иначе по массиву если MAXNUMunlinked <> 2
					leftchild = unlNodes[j];
					//free(unlNodes[0]);
					unlNodes[j] = NULL;
					break;
				}
		}else
			leftchild = doHufNode((*adrFT) + i - 1);
		resSymb.isComposition = 1;//node consists composition of freqs of symbols; "ch" field is senseless
		resSymb.freq = rightchild->symbol.freq + leftchild->symbol.freq;
		result = doHufNode(&resSymb);
		result->left = leftchild;
		result->right = rightchild;
		for (unsigned char j = 0; j < MAXlenARRlinked; j++)
			if (unlNodes[j] == NULL) { 
				unlNodes[j] = (hufNode *)calloc(1, sizeof(hufNode));
				*unlNodes[j] = *result; 
				break; 
			}//must NULL unl nodes[i] when link
		FTDelLastEl(adrFT, &uniqchrs);
		FTDelLastEl(adrFT, &uniqchrs);
		FTadd(adrFT, &resSymb, &uniqchrs, 1); 
		i = uniqchrs - 1;

	}
	return result;
#undef MAXNUMunlinked
}

//dohuftable; to maintenance cpmpression performance
//clean HTable before filling
void fillhuftable(hufTable* HTable, hufNode *HTree, hufTableCell* HCell) {
	hufTableCell nextCell = *HCell;
	if (HTree->left != NULL) {
		nextCell.code[nextCell.HufCodeLen] = b0;
		nextCell.HufCodeLen++;
		fillhuftable(HTable, HTree->left, &nextCell);
	}
	nextCell = *HCell;
	if (HTree->right != NULL) {
		nextCell.code[nextCell.HufCodeLen] = b1;
		nextCell.HufCodeLen++;
		fillhuftable(HTable, HTree->right, &nextCell);
	}
	if (!HTree->symbol.isComposition)
	{
		HTable->table[HTable->len].ch = HTree->symbol.ch;
		HTable->table[HTable->len].HufCodeLen = HCell->HufCodeLen;
		for (unsigned char i = 0; i < HCell->HufCodeLen; i++)
			HTable->table[HTable->len].code[i] = HCell->code[i];
		HTable->len++;
	}
};


void main() {
	unsigned char uniqchrs;
	FILE *f = fopen("173.txt", "rb");
	freqTable *FT = dofreqTable(f, &uniqchrs);
	/* before do huftree save freqtable if needed*/
	hufNode *hufTree = doHufTree(FT, uniqchrs); //seems to work
	hufTableCell *dummyCell = (hufTableCell *)calloc(1, sizeof(hufTableCell));
	fillhuftable(&HTable, hufTree, dummyCell);

	system("pause");
}