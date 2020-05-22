#include <stdio.h>
#include <malloc.h>
#include <string.h>
#include <stddef.h>
#define MAX(a, b) ((a) > (b) ? (a) : (b))
#define foreach(x) for(; (x); (x) = (x)->Next)


typedef uintptr_t pointer;

static int DidError = 0;

static char *
LoadEntireFileNT(char * FileName) {
    char * Result = 0;
    FILE * File = fopen(FileName, "rb");
    if(File) {
        fseek(File, 0, SEEK_END);
        int FileSize = ftell(File);
        fseek(File, 0, SEEK_SET);
        Result = malloc(FileSize + 1);
        if(Result) {
            fread(Result, 1, FileSize, File);
            Result[FileSize] = 0;
        }
    }
    else {
        printf("Failed to read file %s.\n", FileName);
    }

    return Result;
}

/*
    String/Character functions
*/
static int
CharIsAlpha(char c) {
    return (('A' <= c && c <= 'Z') || ('a' <= c && c <= 'z'));
}

static int
CharIsDigit(char c) {
    return ('0' <= c && c <= '9');
}

static int
CharIsSymbol(char c) {
    return c == '.' || c == ':' || c == '/' || c == '\\';
}

static int
StringCompareCaseSensitiveR(const char * String1, const char * String2, int Length) {
    int Match = 0;
    if(String1 && String2 && (Length > 0)) {
        Match = 1;
        for(int i = 0; i < Length; ++i) {
            if(String1[i] != String2[i]) return 0;
        }
    }
    return Match;
}


/*
    Debug
*/
#define Assert(expression) InvalidCode(__LINE__, __FILE__, #expression, expression, 1)
#define SoftAssert(expression) InvalidCode(__LINE__, __FILE__, #expression, expression, 0)

static void
InvalidCode(const int Line, const char * File, const char * Error, int Result, int Crash) {
    if(Result) return;

    printf("Assert Failed: %s\nLine: %i File:%s\n", Error, Line, File);

    if(Crash) {
        __builtin_trap();
        //*((int *)0) = 0;
    }
}

/*
    Memory Arena
*/
static int
IsPowerOfTwo(int x) {
    return (x & (x-1)) == 0;
}

typedef struct memory_arena memory_arena;
struct memory_arena {
    size_t BufferLength;
    size_t PreviousOffset;
    size_t CurrentOffset;
    unsigned char * Buffer;
};

static pointer
AlignForward(pointer Pointer, size_t Alignment) {
    pointer p, a, Modulo;
    Assert(IsPowerOfTwo(Alignment));

    p = Pointer;
    a = (pointer)Alignment;

    //Note(Zen): Identical to p % a. Faster since a = 2^k.
    Modulo = p & (a-1);
    if(Modulo != 0) {
        //Note(Zen): If p is unaligned move address forwards
        p += a - Modulo;
    }
    return p;
}

static void
_ArenaGrow(memory_arena * Arena, size_t NewLength) {
    size_t NewSize = MAX(2 * Arena->BufferLength, NewLength);
    Assert(NewLength <= NewSize);

    Arena->Buffer = realloc(Arena->Buffer, NewSize);
    Arena->BufferLength = NewSize;
}

static void *
_ArenaAllocAligned(memory_arena * Arena, size_t Size, size_t Alignment) {
    pointer CurrentPointer = (pointer)Arena->Buffer + (pointer)Arena->CurrentOffset;
    pointer Offset = AlignForward(CurrentPointer, Alignment);
    Offset -= (pointer)Arena->Buffer;

    if(Offset + Size <= Arena->BufferLength) {
        void * Pointer = &Arena->Buffer[Offset];
        Arena->PreviousOffset = Offset;
        Arena->CurrentOffset = Offset+Size;

        memset(Pointer, 0, Size);
        return Pointer;
    }
    else {
        _ArenaGrow(Arena, Arena->BufferLength + Size);
        return _ArenaAllocAligned(Arena, Size, Alignment);
    }
}

static void *
_ArenaResizeAligned(memory_arena * Arena, void * OldMemory, size_t OldSize, size_t NewSize, size_t Alignment) {
    unsigned char * OldMemoryC = (unsigned char *)OldMemory;

    Assert(IsPowerOfTwo(Alignment));

    if(OldMemoryC == 0 || OldMemoryC == NULL) {
        return _ArenaAllocAligned(Arena, NewSize, Alignment);
    }
    else if((Arena->Buffer <= OldMemoryC) && (OldMemoryC < Arena->Buffer + Arena->BufferLength)) {
        if(Arena->Buffer + Arena->PreviousOffset == OldMemoryC) {
            Arena->CurrentOffset = Arena->PreviousOffset + NewSize;
            if(NewSize > OldSize) {
                memset(&Arena->Buffer[Arena->CurrentOffset], 0, NewSize-OldSize);
            }
            return OldMemory;
        }
        else {
            void * NewMemory = _ArenaAllocAligned(Arena, NewSize, Alignment);
            size_t CopySize = (OldSize < NewSize) ? OldSize : NewSize;
            //Copy across the old memory
            memmove(NewMemory, OldMemory, CopySize);
            return NewMemory;
        }
    }
    else {
        Assert(!"Resized memory is out of bounds in arena.\n");
        return 0;
    }

}

#ifndef DEFAULT_ALIGNMENT
#define DEFAULT_ALIGNMENT (2 * sizeof(void *))
#endif
static void *
ArenaAlloc(memory_arena * Arena, size_t Size) {
    return _ArenaAllocAligned(Arena, Size, DEFAULT_ALIGNMENT);
}


static void *
ArenaResize(memory_arena * Arena, void * OldMemory, size_t OldSize, size_t NewSize) {
    return _ArenaResizeAligned(Arena, OldMemory, OldSize, NewSize, DEFAULT_ALIGNMENT);
}


static void
ArenaInit(memory_arena * Arena, void * BackBuffer, size_t BackBufferLength) {
    Arena->Buffer = (unsigned char *) BackBuffer;
    Arena->BufferLength = BackBufferLength;
    Arena->CurrentOffset = 0;
    Arena->PreviousOffset = 0;
}

static void
ArenaFreeAll(memory_arena * Arena) {
    Arena->CurrentOffset = 0;
    Arena->PreviousOffset = 0;
}

/*
    Dynamic Array/Vector
*/

typedef struct vector_header vector_header;
struct vector_header {
    size_t Length;
    size_t Capacity;
    char Buffer[0];
};

#define _VectorHeader(v) ((vector_header *)((char *)v - offsetof(vector_header, Buffer)))
#define _VectorFits(v, n) (VectorLength(v) + (n) <= VectorCapacity(v))
#define _VectorFit(v, n) (_VectorFits(v, n) ? 0 : ((v) = _VectorGrow((v), VectorLength(v) + (n), sizeof(*(v)))))

#define VectorLength(v) ((v) ? _VectorHeader(v)->Length : 0)
#define VectorCapacity(v) ((v) ? _VectorHeader(v)->Capacity : 0)
#define VectorPushBack(v, x) (_VectorFit(v, 1), v[VectorLength(v)] = (x), _VectorHeader(v)->Length++)
#define VectorFree(v) ((v) ? free(_VectorHeader(v)) : 0)

static void *
_VectorGrow(const void * Vector, size_t NewLength, size_t ElementSize) {
    size_t NewCapacity = MAX(1 + 2 * VectorCapacity(Vector), NewLength);
    Assert(NewLength <= NewCapacity);
    size_t NewSize = offsetof(vector_header, Buffer) + NewCapacity * ElementSize;
    vector_header *NewVector;
    if (Vector) {
        NewVector = realloc(_VectorHeader(Vector), NewSize);
    }
    else {
        NewVector = malloc(NewSize);
        NewVector->Length = 0;
    }
    NewVector->Capacity = NewCapacity;
    return NewVector->Buffer;
}


/*
    Site Generator
*/

typedef enum token_type token_type;
enum token_type {
    TOKEN_LASTCHAR = 127,
    TOKEN_IDENTIFIER,
    TOKEN_TEXT,
    TOKEN_NEW_LINE
};

static int Line;

typedef struct token token;
struct token {
    token_type Type;
    const char * Start;
    const char * End;
    int Line;
};

static void
PrintToken(token Token) {
    if(Token.Type <= TOKEN_LASTCHAR) {
        printf("TOKEN: %c\n", Token.Type);
    }
    else if(Token.Type == TOKEN_IDENTIFIER) {
        printf("TOKEN: IDENTIFIER (%.*s)\n", (int)(Token.End - Token.Start), Token.Start);
    }
    else if(Token.Type == TOKEN_TEXT) {
        printf("TOKEN: TEXT (%.*s)\n", (int)(Token.End - Token.Start), Token.Start);
    }
    else if(Token.Type == TOKEN_NEW_LINE) {
        printf("TOKEN: NEWLINE\n");
    }
}
//WARNING(Zen): This function points to a static string, meaning that multiple calls
//will overwrite the string each time. Make sure before calling again you don't need the return
//of the previous call.
static char *
PrintableTokenName(token Token) {
    static char TokenName[32];
    if(Token.Type <= TOKEN_LASTCHAR) {
        sprintf(TokenName, "character %c.", Token.Type);
    }
    else if(Token.Type == TOKEN_IDENTIFIER) {
        sprintf(TokenName, "%.*s", (int)(Token.End - Token.Start), Token.Start);
    }
    else if(Token.Type == TOKEN_TEXT) {
        sprintf(TokenName, "text");//, (int)(Token.End - Token.Start), Token.Start);
    }
    else if(Token.Type == TOKEN_NEW_LINE) {
        sprintf(TokenName, "newline");
    }
    return TokenName;
}

static char *
AddTokenToBuffer(char * Stream, token ** TokenBuffer) {
    token Token = {0};
    Token.Start = Stream;
    if(*Stream == '@') {
        ++Stream;
        while(CharIsAlpha(*Stream)) {
            ++Stream;
        }
        Token.Type = TOKEN_IDENTIFIER;
    }
    else if(CharIsAlpha(*Stream)) {
        while(CharIsAlpha(*Stream) || CharIsDigit(*Stream) || CharIsSymbol(*Stream) || *Stream == ' ') {
            ++Stream;
        }
        Token.Type = TOKEN_TEXT;
    }
    else if(*Stream == ' ' || *Stream == '\r') {
        //Note(Zen): Skip Spaces as whitespace
        ++Stream;
        return Stream;
    }
    else if(*Stream == '\n') {
        Line++;
        if(*(Stream+1) == '\n' || (*(Stream+1) == '\r' && *(Stream+2) == '\n')) {
            ++Stream;
            Token.Type = TOKEN_NEW_LINE;
        }
        else {
            //Note(Zen): Skip single new lines
            ++Stream;
            return Stream;
        }
    }
    else {
        Token.Type = *Stream++;
    }
    Token.End = Stream;
    Token.Line = Line;
    //PrintToken(Token);
    VectorPushBack((*TokenBuffer), Token);
    return Stream;
}

static int
RequireToken(token_type Type, token Token) {
    if(Token.Type == Type) {
        return 1;
    }
    return 0;
}

static int
MatchToken(token_type Type, token Token) {
    if(Token.Type == Type) {
        return 1;
    }
    return 0;
}

typedef enum page_node_type page_node_type;
enum page_node_type {
    PAGE_NODE_INVALID,
    PAGE_NODE_TITLE,
    PAGE_NODE_SUBTITLE,
    PAGE_NODE_LINK,
    PAGE_NODE_TEXT,
    PAGE_NODE_NEW_PARAGRAPH,
    PAGE_NODE_IMAGE,
    PAGE_NODE_COLUMN
};

typedef struct page_node page_node;
struct page_node {
    page_node_type Type;
    page_node * Next;

    const char * TextStart;
    const char * TextEnd;
    union {
        struct {
            const char * LinkURLStart;
            const char * LinkURLEnd;
        };

        struct {
            page_node * Column1;
            page_node * Column2;
        };
    };
};

static void
PrintPageNode(page_node * PageNode) {
    int TextLength = PageNode->TextEnd - PageNode->TextStart;
    int LinkLength = PageNode->LinkURLEnd - PageNode->LinkURLStart;
    switch (PageNode->Type) {
        case PAGE_NODE_INVALID: {
            printf("PAGE NODE: INVALID\n");
        } break;
        case PAGE_NODE_TITLE: {
            printf("PAGE NODE: TITLE\n\tText: %.*s\n", TextLength, PageNode->TextStart);
        } break;
        case PAGE_NODE_SUBTITLE: {
            printf("PAGE NODE: SUBTITLE\n\tText: %.*s\n", TextLength, PageNode->TextStart);
        } break;
        case PAGE_NODE_LINK: {
            printf("PAGE NODE: LINK\n\tText: %.*s\n\tURL: %.*s\n", TextLength, PageNode->TextStart, LinkLength, PageNode->LinkURLStart);
        } break;
        case PAGE_NODE_TEXT: {
            printf("PAGE NODE: TEXT\n\tText: %.*s\n", TextLength, PageNode->TextStart);
        } break;
        case PAGE_NODE_NEW_PARAGRAPH: {
            printf("PAGE NODE: NEW PARAGRAPH\n");
        } break;
        case PAGE_NODE_IMAGE: {
            printf("PAGE NODE: IMAGE\n\tAlternate Text: %.*s\n\tSource: %.*s\n", TextLength, PageNode->TextStart, LinkLength, PageNode->LinkURLStart);
        } break;
        default: {
            printf("PAGE NODE: @Zen add page node: (%d)\n", PageNode->Type);
        } break;
    }
}

static void
BuildNodeFromTokens() {

}


static void
BuildPage(memory_arena * Arena, token * TokenBuffer) {

    page_node * PreviousNode = 0;
    for(int i = 0; i < VectorLength(TokenBuffer); ++i) {
        token Token = TokenBuffer[i];
        page_node * Node = ArenaAlloc(Arena, sizeof(page_node));

        switch(Token.Type) {
            case TOKEN_NEW_LINE: {
                Node->Type = PAGE_NODE_NEW_PARAGRAPH;
            } break;
            case TOKEN_IDENTIFIER: {
                int TokenLength = (Token.End - Token.Start);
                if(StringCompareCaseSensitiveR(Token.Start, "@Title", TokenLength)) {
                    Node->Type = PAGE_NODE_TITLE;
                    ++i;
                    if(RequireToken('{', TokenBuffer[i])) {
                        ++i;
                        if(RequireToken(TOKEN_TEXT, TokenBuffer[i])) {
                            Node->TextStart = TokenBuffer[i].Start;
                            Node->TextEnd = TokenBuffer[i].End;
                        }
                        else {
                            printf("Error: Argument 1 of @Title should be text, not %s", PrintableTokenName(TokenBuffer[i]));
                            DidError = 1;
                        }
                    }
                    else {
                        printf("Error: @Title should be followed by a '{'\n");
                        DidError = 1;
                    }

                    ++i;
                    if(!RequireToken('}', TokenBuffer[i])) {
                        printf("@Title must be followed by '}'\n");
                        DidError = 1;
                    }
                }
                else if(StringCompareCaseSensitiveR(Token.Start, "@Link", TokenLength)) {
                    Node->Type = PAGE_NODE_LINK;
                    ++i;
                    if(RequireToken('{', TokenBuffer[i])) {
                        ++i;
                        if(RequireToken(TOKEN_TEXT, TokenBuffer[i])) {
                            Node->TextStart = TokenBuffer[i].Start;
                            Node->TextEnd = TokenBuffer[i].End;
                        }
                        else {
                            printf("Error: Argument 1 of @Link should be a string, not %s\n", PrintableTokenName(TokenBuffer[i]));
                            DidError = 1;
                        }

                        ++i;
                        if(!RequireToken(',', TokenBuffer[i])) {
                            printf("Error: @Link requires two arguments\n");
                            DidError = 1;
                        }

                        ++i;
                        if(RequireToken(TOKEN_TEXT, TokenBuffer[i])) {
                            Node->LinkURLStart = TokenBuffer[i].Start;
                            Node->LinkURLEnd = TokenBuffer[i].End;
                        }
                        else {
                            printf("Error: Argument 2 of @Link should be text, not %s\n", PrintableTokenName(TokenBuffer[i]));
                        }
                    }
                    else {
                        printf("Error: Require '{' after @Link\n");
                        DidError = 1;
                    }
                    ++i;
                    if(!RequireToken('}', TokenBuffer[i])) {
                        printf("Error: @Link must be followed by '}'\n");
                        DidError = 1;
                    }
                }
                else if(StringCompareCaseSensitiveR(Token.Start, "@SubTitle", TokenLength)) {
                    Node->Type = PAGE_NODE_SUBTITLE;
                    ++i;
                    if(RequireToken('{', TokenBuffer[i])) {
                        ++i;
                        if(RequireToken(TOKEN_TEXT, TokenBuffer[i])) {
                            Node->TextStart = TokenBuffer[i].Start;
                            Node->TextEnd = TokenBuffer[i].End;
                        }
                        else {
                            printf("Error: Argument 1 of @SubTitle should be text, not %s", PrintableTokenName(TokenBuffer[i]));
                            DidError = 1;
                        }
                    }
                    else {
                        printf("Error: @SubTitle should be followed by a '{'\n");
                        DidError = 1;
                    }

                    ++i;
                    if(!RequireToken('}', TokenBuffer[i])) {
                        printf("@SubTitle must be followed by '}'\n");
                        DidError = 1;
                    }
                }
                else if(StringCompareCaseSensitiveR(Token.Start, "@Image", TokenLength)) {
                    Node->Type = PAGE_NODE_IMAGE;
                    ++i;
                    if(RequireToken('{', TokenBuffer[i])) {
                        ++i;
                        if(RequireToken(TOKEN_TEXT, TokenBuffer[i])) {
                            Node->TextStart = TokenBuffer[i].Start;
                            Node->TextEnd = TokenBuffer[i].End;
                        }
                        else {
                            printf("Error: Argument 1 of @Image should be text, not %s\n", PrintableTokenName(TokenBuffer[i]));
                            DidError = 1;
                        }

                        ++i;
                        if(!RequireToken(',', TokenBuffer[i])) {
                            printf("Error: @Image requires two arguments\n");
                            DidError = 1;
                        }

                        ++i;
                        if(RequireToken(TOKEN_TEXT, TokenBuffer[i])) {
                            Node->LinkURLStart = TokenBuffer[i].Start;
                            Node->LinkURLEnd = TokenBuffer[i].End;
                        }
                        else {
                            printf("Error: Argument 2 of @Image should be text, not %s\n", PrintableTokenName(TokenBuffer[i]));
                            DidError = 1;
                        }

                        ++i;
                        if(!MatchToken('}', TokenBuffer[i])) {
                            printf("Error: @Image must be followed by }\n");
                            DidError = 1;
                        }

                    }
                    else {
                        printf("Error: @Image Requires '{'\n");
                        DidError = 1;
                    }
                }
                else if(StringCompareCaseSensitiveR(Token.Start, "@Column", TokenLength)) {
                    Node->Type = PAGE_NODE_COLUMN;
                    //
                    
                }
                else {
                    printf("Error: Unknown Identifier (%.*s)\n", (int)(Token.End-Token.Start), Token.Start);
                    DidError = 1;
                }
            } break;
            case TOKEN_TEXT: {
                Node->Type = PAGE_NODE_TEXT;
                Node->TextStart = Token.Start;
                Node->TextEnd = Token.End;
            } break;
            default: {
                printf("Error: Couldn't build page node from token %d: %s\n", i, PrintableTokenName(Token));
            } break;
        }
        if(PreviousNode) {
            PreviousNode->Next = Node;
        }
        PreviousNode = Node;
    }
    //TODO(Zen): Only if in debug mode
    page_node * Node = (page_node *)Arena->Buffer;
    foreach(Node) {
        PrintPageNode(Node);
    }
}

typedef struct site_info site_info;
struct site_info {
    char * AuthorName;
    char * PageTitle;
};

static void
OutputPageNodeAsHTML(FILE * OutputFile, page_node * CurrentNode, int * InParagraph) {
    switch (CurrentNode->Type) {
        case PAGE_NODE_TITLE: {
            int TextLength = (int)(CurrentNode->TextEnd-CurrentNode->TextStart);

            fprintf(OutputFile, "<h1>%.*s</h1>\n", TextLength, CurrentNode->TextStart);
        } break;
        case PAGE_NODE_NEW_PARAGRAPH: {
            if(*InParagraph) {
                fprintf(OutputFile, "\n</p>\n");
                *InParagraph = 0;
            }
        } break;
        case PAGE_NODE_LINK: {
            int TextLength = CurrentNode->TextEnd - CurrentNode->TextStart;
            int LinkLength = CurrentNode->LinkURLEnd - CurrentNode->LinkURLStart;
            fprintf(OutputFile, "<a href='%.*s' class='Link'>%.*s</a> ", LinkLength, CurrentNode->LinkURLStart, TextLength, CurrentNode->TextStart);
        } break;
        case PAGE_NODE_TEXT: {
            if(!InParagraph) fprintf(OutputFile, "<p>\n");

            int TextLength = CurrentNode->TextEnd - CurrentNode->TextStart;
            fprintf(OutputFile, "%.*s", TextLength, CurrentNode->TextStart);
            *InParagraph = 1;
        } break;
        case PAGE_NODE_SUBTITLE: {
            int TextLength = (int)(CurrentNode->TextEnd-CurrentNode->TextStart);
            fprintf(OutputFile, "<h2>%.*s</h2>\n", TextLength, CurrentNode->TextStart);
        } break;
        case PAGE_NODE_IMAGE: {
            int TextLength = CurrentNode->TextEnd - CurrentNode->TextStart;
            int LinkLength = CurrentNode->LinkURLEnd - CurrentNode->LinkURLStart;
            fprintf(OutputFile, "<img src='%.*s' alt='%.*s' width='100%%'>\n", LinkLength, CurrentNode->LinkURLStart, TextLength, CurrentNode->TextStart);
        } break;
        default: {
            printf("@Zen add content for page node (%d)\n", CurrentNode->Type);
        }
    }
}

static void
OutputPageNodeListAsHTML(site_info SiteInfo, memory_arena * NodesArena, char * Header, char * Footer) {
    //Note(Zen): Output to the file
    //TODO(Zen): make the out the file name that went in
    FILE * OutputFile = fopen("out.html", "w");

    fprintf(OutputFile, "<!DOCTYPE html>\n");
    fprintf(OutputFile, "<!-- Auto generated by Zenarii's Static Website Generator: https://github.com/Zenarii-->\n");
    fprintf(OutputFile, "<html lang='en'>\n");
    fprintf(OutputFile, "<head>\n");
    {
        if(SiteInfo.PageTitle) fprintf(OutputFile, "<title>%s</title>\n", SiteInfo.PageTitle);

        fprintf(OutputFile, "<link rel='stylesheet' type='text/css' href='style.css'>\n");
        fprintf(OutputFile, "<meta name='viewport' content='width=device-width, initial-scale=1.0'>\n");
        fprintf(OutputFile, "<meta charset='UTF-8'>\n");
        //TODO(Zen): Open graph stuff e.g. og:title https://ogp.me/
        if(SiteInfo.AuthorName) fprintf(OutputFile, "<meta name='author' content='%s'>\n", SiteInfo.AuthorName);
    }
    fprintf(OutputFile, "</head>\n");

    fprintf(OutputFile, "<body>\n");
    {
        if(Header) fprintf(OutputFile, "%s", Header);
        int InParagraph = 0;
        page_node * Node = (page_node *)NodesArena->Buffer;

        for(; Node; Node = Node->Next) {
            OutputPageNodeAsHTML(OutputFile, Node, &InParagraph);
        }
        if(InParagraph) {
            fprintf(OutputFile, "\n</p>\n");
        }

        if(Footer) fprintf(OutputFile, "%s", Footer);
    }
    fprintf(OutputFile, "</body>\n");

    fprintf(OutputFile, "</html>\n");
    fclose(OutputFile);

    printf("Program reached end.\n");
}

#define MAX_FILES_TO_PARSE 4
#define OUTPUT_HTML (1<<0)

int
main(int argc, char ** args) {
    site_info SiteInfo = {0};
    char * HeaderPath = 0;
    char * FooterPath = 0;
    char * Header = 0;
    char * Footer = 0;
    int FileCount = 0;
    char * FilesToParse[MAX_FILES_TO_PARSE] = {0};
    unsigned int Output = 0;
    //Note(Zen): Parse arguments sent to the program.
    for(int i = 1; i < argc; ++i) {
        //Note(Zen): Single args
        if(strcmp(args[i], "--html") == 0) {
            printf("Outputting as html.\n");
            args[i] = 0;
            Output |= OUTPUT_HTML;
        }
        //Note(Zen): Arguments with some extra info
        else if(strcmp(args[i], "--header") == 0) {
            HeaderPath = args[i+1];
            args[i] = 0;
            args[i+1] = 0;
            ++i;
            printf("Header path is %s.\n", HeaderPath);
        }
        else if(strcmp(args[i], "--footer") == 0) {
            FooterPath = args[i+1];
            args[i] = 0;
            args[i+1] = 0;
            ++i;
            printf("Footer path is %s.\n", FooterPath);
        }
        else if(strcmp(args[i], "--author") == 0) {
            SiteInfo.AuthorName = args[i+1];
            args[i] = 0;
            args[i+1] = 0;
            ++i;
            printf("Author Name is %s.\n", SiteInfo.AuthorName);
        }
        else if(strcmp(args[i], "--title") == 0) {
            SiteInfo.PageTitle = args[i+1];
            args[i] = 0;
            args[i+1] = 0;
            ++i;
            printf("Page title is %s.\n", SiteInfo.PageTitle);
        }
    }

    for(int i = 1; i < argc; ++i) {
        if(args[i]) {
            FilesToParse[FileCount++] = args[i];
        }
    }

    if(HeaderPath) {
        Header = LoadEntireFileNT(HeaderPath);
    }
    if(FooterPath) {
        Footer = LoadEntireFileNT(Footer);
    }

    //Note(Zen): Build a token buffer for each file

    memory_arena PageNodesArena = {0};
    void * ArenaMemory = malloc(32 * sizeof(page_node));
    ArenaInit(&PageNodesArena, ArenaMemory, 32 * sizeof(page_node));

    for(int i = 0; i < FileCount; ++i) {
        char * File = LoadEntireFileNT(FilesToParse[i]);
        char * Stream = File;
        //Note(Zen): Parse the file.
        printf("Parsing %s.\n", FilesToParse[i]);
        token * TokenBuffer = NULL;
        while(*Stream) {
            Stream = AddTokenToBuffer(Stream, &TokenBuffer);
        }
        for(int i = 0; i < VectorLength(TokenBuffer); ++i) {
            printf("%d", i);
            PrintToken(TokenBuffer[i]);
        }
        //Note(Zen): Build page tree.
        printf("\nBuilding Page Tree:\n");

        BuildPage(&PageNodesArena, TokenBuffer);

        if(!DidError) {
            if(Output & OUTPUT_HTML) {
                OutputPageNodeListAsHTML(SiteInfo, &PageNodesArena, Header, Footer);
            }
        }
        else {
            printf("Failed to parse file: %s, due to errors.\n", FilesToParse[i]);
        }
        ArenaFreeAll(&PageNodesArena);
        //Note(Zen): Reset Globals
        Line = 0;
        DidError = 0;
    }
    printf("Reached end of program without crashing.\n");
    return 0;
}
