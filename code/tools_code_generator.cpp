#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <memory.h>
#include <string.h>

#include "engine_meta.h"

typedef uint8_t u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;
typedef uintptr_t uptr;
typedef intptr_t iptr;
typedef short i16;
typedef int i32;
typedef long long i64;
typedef float f32;
typedef double f64;
#define ptrcast(type, expr) reinterpret_cast<type*>(expr)
#define Assert(expr) if (!(expr)) *(char*)(0) = 0

enum TokenType {
	Token_Unknown,

	Token_Identifier,
	Token_String,
	Token_Number,
	Token_Asterix,			// *
	Token_Amper,			// &
	Token_Colon,			// :
	Token_SemiColon,		// ;
	Token_OpenBrace,		// [
	Token_CloseBrace,		// ]
	Token_OpenBracket,		// {
	Token_CloseBracket,		// }
	Token_OpenParen,		// (
	Token_CloseParen,		// )

	Token_EndOfStream
};

struct String {
	char* text;
	u32 size;
};

struct Token {
	TokenType type;
	String str;
};

struct IntrospectableMemberDefinition {
	String type;
	String name;
	u32 flags;

	IntrospectableMemberDefinition* next;
};

struct IntrospectableMemberDefinitionList {
	String typeName;
	IntrospectableMemberDefinition* list;

	IntrospectableMemberDefinitionList* next;
};

struct Tokenizer {
	char* at;

	IntrospectableMemberDefinitionList* definitions;
};

char* ReadEntireFileAndNullTerminate(const char* filename) {
	FILE* file;
	fopen_s(&file, filename, "r");
	if (!file) {
		return 0;
	}
	fseek(file, 0, SEEK_END);
	size_t size = ftell(file);
	fseek(file, 0, SEEK_SET);
	char* content = ptrcast(char, malloc(size + 1));
	fread(content, size, 1, file);
	content[size] = 0;
	return content;
}

inline
bool IsEndLine(char c) {
	bool result = (c == '\n') ||
				  (c == '\r');
	return result;
}

inline
bool IsWhiteSpace(char c) {
	bool result = (c == ' ') ||
				  (c == '\t') ||
				  IsEndLine(c);
	return result;
}

inline
bool IsEndOfStream(char c) {
	return c == 0;
}

inline
bool IsNumber(char c) {
	bool result = (c >= '0' && c <= '9');
	return result;
}

inline
bool IsLetter(char c) {
	bool result = (c >= 'a' && c <= 'z') ||
				  (c >= 'A' && c <= 'Z');
	return result;
}

inline
bool IsNextInStreamEqualTo(Tokenizer& tokenizer, char c) {
	return tokenizer.at[0] == c;
}


bool StringsAreEqual(String& string, const char* nullterminated) {
	char* text = string.text;
	for (u32 idx = 0; idx < string.size; idx++) {
		if (IsEndOfStream(nullterminated[0]) || *text++ != *nullterminated++) {
			return false;
		}
	}
	bool result = IsEndOfStream(nullterminated[0]);
	return result;
}


inline
String ParseU32(Tokenizer& tokenizer) {
	Assert(IsNumber(tokenizer.at[0]));
	String result = {};
	result.text = tokenizer.at;
	while (tokenizer.at[0] && (
		IsNumber(tokenizer.at[0])
		)) {
		tokenizer.at++;
	}
	result.size = u32(tokenizer.at - result.text);
	return result;
}
// 23213
// 1.0
// 1.f
// 1.000f
// -3213
inline
String ParseNumber(Tokenizer& tokenizer) {
	String result = {};
	result.text = tokenizer.at;
	if (IsNextInStreamEqualTo(tokenizer, '-')) {
		tokenizer.at++;
	}
	ParseU32(tokenizer);
	if (IsNextInStreamEqualTo(tokenizer, '.')) {
		tokenizer.at++;
		if (IsNumber(tokenizer.at[0])) {
			ParseU32(tokenizer);
		}
		if (IsNextInStreamEqualTo(tokenizer, 'f')) {
			tokenizer.at++;
			// Float;
		}
	}
	result.size = u32(tokenizer.at - result.text);
	Assert(result.size > 0);
	return result;
}

inline
String ParseIdentifier(Tokenizer& tokenizer) {
	String result = {};
	result.text = tokenizer.at;
	while (tokenizer.at[0] && (
		IsLetter(tokenizer.at[0]) || IsNumber(tokenizer.at[0]) || tokenizer.at[0] == '_'
		)) {
		tokenizer.at++;
	}
	result.size = u32(tokenizer.at - result.text);
	Assert(result.size > 0);
	return result;
}

String ParseCStyleComment(Tokenizer& tokenizer) {
	Assert(tokenizer.at[0] == '/' && tokenizer.at[1] == '/');
	tokenizer.at += 2;
	String result = {};
	result.text = tokenizer.at;
	while (tokenizer.at[0] && !IsEndLine(tokenizer.at[0])) {
		tokenizer.at++;
	}
	result.size = u32(tokenizer.at - result.text);
	return result;
}

String ParseCppStyleComment(Tokenizer& tokenizer) {
	Assert(tokenizer.at[0] == '/' && tokenizer.at[1] == '*');
	tokenizer.at += 2;
	String result = {};
	result.text = tokenizer.at;
	while (tokenizer.at[0] && tokenizer.at[0] == '*') {
		tokenizer.at++;
		if (tokenizer.at[0] == '/') {
			tokenizer.at++;
			break;
		}
	}
	result.size = u32(tokenizer.at - result.text - 2);
	return result;
}

String ParseString(Tokenizer& tokenizer) {
	Assert(tokenizer.at[0] == '"');
	char* strStart = ++tokenizer.at;
	while (tokenizer.at[0] && tokenizer.at[0] != '"') {
		tokenizer.at++;
		if (tokenizer.at[0] == '\\' && tokenizer.at[1]) {
			tokenizer.at += 2;
		}
	}
	if (tokenizer.at[0]) {
		tokenizer.at++;
	}
	String result = {};
	result.size = u32(tokenizer.at - strStart);
#if 0
	result.text = ptrcast(char, malloc(result.size));
	memcpy(result.text, strStart, result.size * sizeof(char));
#else
	result.text = strStart;
#endif
	return result;
}

Token GetToken(Tokenizer& tokenizer) {
	while (IsWhiteSpace(tokenizer.at[0])) {
		tokenizer.at++;
	}
	
	Token token = {};
	token.str.text = tokenizer.at;
	token.str.size = 1;
	char c = tokenizer.at[0];
	switch (c) {
	case 0: { token.type = Token_EndOfStream; tokenizer.at++;} break;
	case '*': { token.type = Token_Asterix; tokenizer.at++;} break;
	case '&': { token.type = Token_Amper; tokenizer.at++; } break;
	case ':': { token.type = Token_Colon; tokenizer.at++;} break;
	case ';': { token.type = Token_SemiColon; tokenizer.at++;} break;
	case '[': { token.type = Token_OpenBrace; tokenizer.at++;} break;
	case ']': { token.type = Token_CloseBrace; tokenizer.at++;} break;
	case '{': { token.type = Token_OpenBracket; tokenizer.at++;} break;
	case '}': { token.type = Token_CloseBracket; tokenizer.at++;} break;
	case '(': { token.type = Token_OpenParen; tokenizer.at++;} break;
	case ')': { token.type = Token_CloseParen; tokenizer.at++;} break;
	case '/': {
		if (IsNextInStreamEqualTo(tokenizer, '/')) {
			ParseCStyleComment(tokenizer);
			token = GetToken(tokenizer);
		}
		else if (IsNextInStreamEqualTo(tokenizer, '*')) {
			ParseCppStyleComment(tokenizer);
			token = GetToken(tokenizer);
		}
		else {
			tokenizer.at++;
		}
	} break;
	case '"': {
		token.type = Token_String;
		token.str = ParseString(tokenizer);
	} break;
	default: {
		if (IsNumber(c) || c == '-') {
			token.type = Token_Number;
			token.str = ParseNumber(tokenizer);
		}
		else if (IsLetter(c)) {
			token.type = Token_Identifier;
			token.str = ParseIdentifier(tokenizer);
		}
		else {
			tokenizer.at++;
			//fprintf(stderr, "Unkown token %c \n", c);
		}
	} break;
	}

	return token;
}

Token ExpectToken(Tokenizer& tokenizer, TokenType type) {
	Token token = GetToken(tokenizer);
	Token copyForStupidVisualStudio = token;
	Assert(token.type == type);
	return token;
}

Token CheckNextTokenWithoutAdvance(Tokenizer& tokenizer) {
	char* state = tokenizer.at;
	Token token = GetToken(tokenizer);
	tokenizer.at = state;
	return token;
}

void RecursiveParseMemberDefinition(Tokenizer& tokenizer, IntrospectableMemberDefinition* member) {
	Token token = GetToken(tokenizer);
	if (token.type == Token_Asterix) {
		// TODO: Pointer to pointer to pointer to pointer...
		member->flags |= MemberDefinitionFlag_IsPointer;
		return RecursiveParseMemberDefinition(tokenizer, member);
	}
	else if (token.type == Token_Amper) {
		Assert((member->flags & MemberDefinitionFlag_IsReference) == 0);
		member->flags |= MemberDefinitionFlag_IsReference;
		return RecursiveParseMemberDefinition(tokenizer, member);
	}
	else if (token.type == Token_Identifier) {
		member->name = token.str;
	}
	else {
		Assert(!"Invalid Token!");
	}
}

void ParseIntrospectable(Tokenizer& tokenizer) {
	ExpectToken(tokenizer, Token_OpenParen);
	ExpectToken(tokenizer, Token_Identifier); // Category
	ExpectToken(tokenizer, Token_Colon);
	ExpectToken(tokenizer, Token_String);
	ExpectToken(tokenizer, Token_CloseParen);
	Token token = GetToken(tokenizer);
	// Other types than struct not supported;
	if (token.type == Token_Identifier && StringsAreEqual(token.str, "struct")) {
		IntrospectableMemberDefinitionList* definition = ptrcast(IntrospectableMemberDefinitionList, malloc(sizeof(IntrospectableMemberDefinitionList)));
		definition->typeName = ExpectToken(tokenizer, Token_Identifier).str;
		definition->next = tokenizer.definitions;
		definition->list = 0;
		tokenizer.definitions = definition;
		token = ExpectToken(tokenizer, Token_OpenBracket);
		while (CheckNextTokenWithoutAdvance(tokenizer).type != Token_CloseBracket) {
			IntrospectableMemberDefinition* member = ptrcast(IntrospectableMemberDefinition, malloc(sizeof(IntrospectableMemberDefinition)));
			member->flags = 0;
			member->type = ExpectToken(tokenizer, Token_Identifier).str;
			RecursiveParseMemberDefinition(tokenizer, member);
			member->next = definition->list;
			definition->list = member;
			ExpectToken(tokenizer, Token_SemiColon);
		}
	}
}

int main() {
	Tokenizer tokenizer = {};
	tokenizer.at = ReadEntireFileAndNullTerminate("engine_world.h");

	bool running = true;
	while (running) {
#if 1
		if (tokenizer.at[0] == 'I'
			&& tokenizer.at[1] == 'n'
			&& tokenizer.at[2] == 't'
			&& tokenizer.at[3] == 'r'
			&& tokenizer.at[4] == 'o') {
			int br = 0;
		}
#endif
		Token token = GetToken(tokenizer);
#if 1
		if (token.type == Token_Identifier && token.str.text[0] == 'I'
			&& token.str.text[1] == 'n'
			&& token.str.text[2] == 't'
			&& token.str.text[3] == 'r'
			&& token.str.text[4] == 'o') {
			int br = 0;
		}
		if (StringsAreEqual(token.str, "struct")) {
			int br = 0;
		}
#endif

		switch (token.type) {
		case Token_Unknown: {
		} break;
		case Token_EndOfStream: {
			running = false;
		} break;
		case Token_Identifier: {
			if (StringsAreEqual(token.str, "Introspect")) {
				ParseIntrospectable(tokenizer);
			}
		}
		default: {
			//printf("%d, %.*s\n", token.type, token.str.size, token.str.text);
		} break;
		}
	}

	printf("#pragma once\n");
	printf("#include \"engine_meta.h\"\n\n");
	for (IntrospectableMemberDefinitionList* definition = tokenizer.definitions;
		definition;
		definition = definition->next
		) {
		String& type = definition->typeName;
		printf("MemberDefinition MembersOf_%.*s[] = {\n", type.size, type.text);
		for (IntrospectableMemberDefinition* member = definition->list; member; member = member->next) {
			String& mtype = member->type;
			String& mname = member->name;
			printf("	{ MetaType_%.*s, \"%.*s\", u32(reinterpret_cast<uptr>(&(((%.*s*)0)->%.*s))), %d },\n", 
				mtype.size, mtype.text, mname.size, mname.text,
				type.size, type.text, mname.size, mname.text, 
				member->flags
			);
		}
		printf("};\n\n");
	}
}