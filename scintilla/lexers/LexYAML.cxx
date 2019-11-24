// This file is part of Notepad2.
// See License.txt for details about distribution and modification.
//! Lexer for YAML.

#include <cassert>
#include <cstring>
#include <cctype>

#include "ILexer.h"
#include "Scintilla.h"
#include "SciLexer.h"

#include "WordList.h"
#include "LexAccessor.h"
#include "Accessor.h"
#include "StyleContext.h"
#include "CharacterSet.h"
#include "LexerModule.h"

using namespace Scintilla;

namespace {

struct EscapeSequence {
	int digitsLeft = 0;

	// highlight any character as escape sequence.
	void resetEscapeState(int chNext) noexcept {
		digitsLeft = 1;
		if (chNext == 'x') {
			digitsLeft = 3;
		} else if (chNext == 'u') {
			digitsLeft = 5;
		} else if (chNext == 'U') {
			digitsLeft = 9;
		}
	}
	bool atEscapeEnd(int ch) noexcept {
		--digitsLeft;
		return digitsLeft <= 0 || !IsHexDigit(ch);
	}
};

constexpr bool IsYAMLFlowIndicator(int ch) noexcept {
	// c-flow-indicator
	return ch == ',' || ch == '[' || ch == ']' || ch == '{' || ch == '}';
}

constexpr bool IsYAMLOperator(int ch) noexcept {
	// remaining c-indicator
	return IsYAMLFlowIndicator(ch) || ch == '@' || ch == '`';
}

inline bool IsYAMLAnchorChar(int ch) noexcept {
	// ns-anchor-char ::= ns-char - c-flow-indicator
	return ch > 0x7f || (isgraph(ch) && !IsYAMLFlowIndicator(ch));
}

constexpr bool IsYAMLDateTime(int ch, int chNext) noexcept {
	return ((ch == '-' || ch == ':' || ch == '.') && IsADigit(chNext))
		|| (ch == ' ' && (chNext == '-' || IsADigit(chNext)));
}

bool IsYAMLText(StyleContext& sc, Sci_Position lineStartNext, int braceCount, const WordList *kwList) noexcept {
	const int state = sc.state;
	const Sci_Position endPos = braceCount? sc.styler.Length() : lineStartNext;
	const int chNext = LexGetNextChar(sc.currentPos, endPos, sc.styler);
	if (chNext == ':') {
		sc.ChangeState(SCE_YAML_TEXT);
		return true;
	}
	if (chNext == '\0'
		|| (chNext == '#' && isspacechar(sc.ch))
		|| (braceCount && (chNext == ',' || chNext == '}' || chNext == ']'))) {
		if (state == SCE_YAML_IDENTIFIER) {
			char s[8];
			sc.GetCurrentLowered(s, sizeof(s));
			if (kwList->InList(s)) {
				sc.ChangeState(SCE_YAML_KEYWORD);
				sc.SetState(SCE_YAML_DEFAULT);
			}
		} else {
			sc.SetState(SCE_YAML_DEFAULT);
		}
	}
	if (sc.state == state) {
		sc.ChangeState(SCE_YAML_TEXT);
		return true;
	}
	return false;
}

enum {
	YAMLLineType_None = 0,
	YAMLLineType_EmptyLine = 1,
	YAMLLineType_CommentLine = 2,
	YAMLLineType_DocumentStart = 3,
	YAMLLineType_DocumentEnd = 4,

	YAMLLineStateMask_IndentCount = 0xfff,
};

void ColouriseYAMLDoc(Sci_PositionU startPos, Sci_Position lengthDoc, int initStyle, LexerWordList keywordLists, Accessor &styler) {
	// ns-uri-char
	const CharacterSet setUriChar(CharacterSet::setAlphaNum, "%-#;/?:@&=+$,_.!~*'()[]");

	int visibleChars = 0;
	int indentCount = 0;
	int textIndentCount = 0;
	int braceCount = 0;
	int lineType = YAMLLineType_None;
	EscapeSequence escSeq;

	StyleContext sc(startPos, lengthDoc, initStyle, styler);
	if (sc.currentLine > 0) {
		const int lineState = styler.GetLineState(sc.currentLine - 1);
		/*
		7: braceCount
		9: textIndentCount
		12: indentCount
		3: lineType
		*/
		braceCount = lineState & 0x7f;
		textIndentCount = (lineState >> 7) & 0x1ff;
	}

	Sci_Position lineStartNext = styler.LineStart(sc.currentLine + 1);

	while (sc.More()) {
		if (sc.atLineStart) {
			lineStartNext = styler.LineStart(sc.currentLine + 1);
			visibleChars = 0;
			indentCount = 0;
			if (sc.state == SCE_YAML_TEXT_BLOCK) {
				Sci_Position pos = sc.currentPos;
				char ch = '\n';
				while (pos < lineStartNext && (ch = styler[pos]) == ' ') {
					++pos;
					++indentCount;
				}
				if (sc.state == SCE_YAML_TEXT_BLOCK && indentCount <= textIndentCount && !(ch == '\n' || ch == '\r')) {
					textIndentCount = 0;
					sc.SetState(SCE_YAML_DEFAULT);
				}
				sc.Forward(indentCount);
			}
		}

		switch (sc.state) {
		case SCE_YAML_OPERATOR:
			sc.SetState(SCE_YAML_DEFAULT);
			break;

		case SCE_YAML_NUMBER:
			if (!IsDecimalNumber(sc.chPrev, sc.ch, sc.chNext)) {
				if (IsYAMLDateTime(sc.ch, sc.chNext)) {
					sc.ChangeState(SCE_YAML_DATETIME);
				} else if (IsYAMLText(sc, lineStartNext, braceCount, nullptr)) {
					continue;
				}
			}
			break;

		case SCE_YAML_DATETIME:
			if (!(IsIdentifierChar(sc.ch) || IsYAMLDateTime(sc.ch, sc.chNext))) {
				 if (IsYAMLText(sc, lineStartNext, braceCount, nullptr)) {
					continue;
				}
			}
			break;

		case SCE_YAML_IDENTIFIER:
			if (!IsAlpha(sc.ch)) {
				if (IsYAMLText(sc, lineStartNext, braceCount, keywordLists[0])) {
					continue;
				}
			}
			break;

		case SCE_YAML_TEXT:
			if (sc.atLineStart && !braceCount) {
				sc.SetState(SCE_YAML_DEFAULT);
			} else if (sc.ch == ':') {
				if (isspacechar(sc.chNext)) {
					sc.ChangeState(SCE_YAML_KEY);
					sc.SetState(SCE_YAML_OPERATOR);
				}
			} else if (braceCount && IsYAMLFlowIndicator(sc.ch)) {
				sc.SetState(SCE_YAML_OPERATOR);
				if (sc.ch == '{' || sc.ch == '[') {
					++braceCount;
				} else if (sc.ch == '}' || sc.ch == ']') {
					--braceCount;
				}
			} else if (sc.ch == '#' && isspacechar(sc.chPrev)) {
				sc.SetState(SCE_YAML_COMMENT);
			}
			break;

		case SCE_YAML_REFERENCE:
			if (!IsYAMLAnchorChar(sc.ch)) {
				sc.SetState(SCE_YAML_DEFAULT);
			}
			break;

		case SCE_YAML_TAG:
		case SCE_YAML_VERBATIM_TAG:
			if (sc.state == SCE_YAML_VERBATIM_TAG && sc.ch == '>') {
				sc.ForwardSetState(SCE_YAML_DEFAULT);
			} else if (!setUriChar.Contains(sc.ch)) {
				sc.SetState(SCE_YAML_DEFAULT);
			}
			break;

		case SCE_YAML_STRING1:
			if (sc.ch == '\'') {
				if (sc.chNext == '\'') {
					sc.SetState(SCE_YAML_ESCAPECHAR);
					sc.Forward(2);
				} else {
					sc.Forward();
					if (sc.GetNextNSChar() == ':') {
						sc.ChangeState(SCE_YAML_KEY);
					}
					sc.SetState(SCE_YAML_DEFAULT);
				}
			}
			break;

		case SCE_YAML_STRING2:
			if (sc.ch == '\\') {
				escSeq.resetEscapeState(sc.chNext);
				sc.SetState(SCE_YAML_ESCAPECHAR);
				sc.Forward();
			} else if (sc.ch == '\"') {
				sc.Forward();
				if (sc.GetNextNSChar() == ':') {
					sc.ChangeState(SCE_YAML_KEY);
				}
				sc.SetState(SCE_YAML_DEFAULT);
			}
			break;

		case SCE_YAML_ESCAPECHAR:
			if (escSeq.atEscapeEnd(sc.ch)) {
				if (sc.ch == '\\') {
					escSeq.resetEscapeState(sc.chNext);
					sc.Forward();
				} else {
					sc.SetState(SCE_YAML_STRING2);
					continue;
				}
			}
			break;

		case SCE_YAML_COMMENT:
		case SCE_YAML_DOCUMENT:
		case SCE_YAML_DIRECTIVE:
			if (sc.atLineStart) {
				sc.SetState(SCE_YAML_DEFAULT);
			}
			break;
		}

		if (sc.state == SCE_YAML_DEFAULT) {
			if (sc.ch == '%' && visibleChars == 0) {
				sc.SetState(SCE_YAML_DIRECTIVE);
			} else if (sc.ch == '#' && (visibleChars == 0 || isspacechar(sc.chPrev))) {
				sc.SetState(SCE_YAML_COMMENT);
				if (visibleChars == 0) {
					lineType = YAMLLineType_CommentLine;
				}
			} else if (visibleChars == 0 && (sc.Match("---") || sc.Match("..."))) {
				braceCount = 0;
				visibleChars = 1;
				//lineType = (sc.ch == '-')? YAMLLineType_DocumentStart : YAMLLineType_DocumentEnd;
				sc.SetState(SCE_YAML_DOCUMENT);
				sc.Forward(3);
				const int chNext = LexGetNextChar(sc.currentPos + 1, lineStartNext, styler);
				if (chNext != '\0') {
					sc.SetState(SCE_YAML_DEFAULT);
				}
			} else if (sc.ch == '\'') {
				sc.SetState(SCE_YAML_STRING1);
			} else if (sc.ch == '\"') {
				sc.SetState(SCE_YAML_STRING2);
			} else if ((sc.ch == '&' || sc.ch == '*') && IsYAMLAnchorChar(sc.chNext)) {
				sc.SetState(SCE_YAML_REFERENCE);
			} else if (sc.ch == '!') {
				if (sc.chNext == '<') {
					sc.SetState(SCE_YAML_VERBATIM_TAG);
					sc.Forward(2);
				} else {
					sc.SetState(SCE_YAML_TAG);
				}
			} else if (sc.ch == '|' || sc.ch == '>') {
				// ignore block scalar header or comment
				textIndentCount = indentCount;
				sc.SetState(SCE_YAML_TEXT_BLOCK);
			} else if (IsADigit(sc.ch) || (sc.ch == '.' && IsADigit(sc.chNext))) {
				sc.SetState(SCE_YAML_NUMBER);
			} else if (IsAlpha(sc.ch) || (sc.ch == '.' && IsAlpha(sc.chNext))) {
				sc.SetState(SCE_YAML_IDENTIFIER);
			} else if (IsYAMLOperator(sc.ch) || (sc.ch == '?' && sc.chPrev == ' ')) {
				sc.SetState(SCE_YAML_OPERATOR);
				if (sc.ch == '{' || sc.ch == '[') {
					++braceCount;
				} else if (sc.ch == '}' || sc.ch == ']') {
					--braceCount;
				}
			} else if (sc.ch == '+' || sc.ch == '-' || sc.ch == '.') {
				if ((sc.ch == '-' && isspacechar(sc.chNext))
					|| IsADigit(sc.chNext)
					|| (sc.ch != '.' && sc.chNext == '.')) {
					sc.SetState(SCE_YAML_OPERATOR);
				} else {
					sc.SetState(SCE_YAML_TEXT);
				}
			} else if (!isspacechar(sc.ch)) {
				sc.SetState(SCE_YAML_TEXT);
			}
		}

		if (visibleChars == 0) {
			if (sc.ch == ' ') {
				++indentCount;
			} else if (!(sc.ch == '\n' || sc.ch == '\r')) {
				++visibleChars;
			}
		}
		if (sc.atLineEnd) {
			if (sc.state == SCE_YAML_TEXT_BLOCK) {
				if (indentCount != textIndentCount) {
					// inside block scalar
					indentCount = textIndentCount + 1;
				}
			} else if (visibleChars == 0) {
				indentCount = 0;
				lineType = YAMLLineType_EmptyLine;
			}

			const int lineState = braceCount | (textIndentCount << 7) | (indentCount << 16) | (lineType << 28);
			styler.SetLineState(sc.currentLine, lineState);
			lineType = YAMLLineType_None;
		}
		sc.Forward();
	}

	sc.Complete();
}

struct FoldLineState {
	int indentCount;
	int lineType;
	constexpr explicit FoldLineState(int lineState) noexcept:
		indentCount((lineState >> 16) & YAMLLineStateMask_IndentCount),
		lineType(lineState >> 28) {
	}
	constexpr bool Empty() const noexcept {
		return lineType == YAMLLineType_EmptyLine || lineType == YAMLLineType_CommentLine;
	}
};

void FoldYAMLDoc(Sci_PositionU startPos, Sci_Position lengthDoc, int /*initStyle*/, LexerWordList, Accessor &styler) {
	const Sci_Position maxPos = startPos + lengthDoc;
	const Sci_Position docLines = styler.GetLine(styler.Length());
	const Sci_Position maxLines = (maxPos == styler.Length()) ? docLines : styler.GetLine(maxPos - 1);

	Sci_Position lineCurrent = styler.GetLine(startPos);
	FoldLineState stateCurrent(styler.GetLineState(lineCurrent));
	while (lineCurrent > 0) {
		lineCurrent--;
		stateCurrent = FoldLineState(styler.GetLineState(lineCurrent));
		if (!stateCurrent.Empty()) {
			break;
		}
	}

	while (lineCurrent <= maxLines) {
		Sci_Position lineNext = lineCurrent + 1;
		FoldLineState stateNext = stateCurrent;
		if (lineNext <= docLines) {
			stateNext = FoldLineState(styler.GetLineState(lineNext));
		}
		if (stateNext.Empty()) {
			stateNext.indentCount = stateCurrent.indentCount;
		}
		while ((lineNext < docLines) && stateNext.Empty()) {
			lineNext++;
			stateNext = FoldLineState(styler.GetLineState(lineNext));
		}

		const int indentCurrentLevel = stateCurrent.indentCount;
		const int levelAfterBlank = stateNext.indentCount;
		const int levelBeforeBlank = (indentCurrentLevel > levelAfterBlank) ? indentCurrentLevel : levelAfterBlank;

		Sci_Position skipLine = lineNext;
		int skipLevel = levelAfterBlank;

		while (--skipLine > lineCurrent) {
			const FoldLineState skipLineState = FoldLineState(styler.GetLineState(skipLine));
			if (skipLineState.indentCount > levelAfterBlank && !skipLineState.Empty()) {
				skipLevel = levelBeforeBlank;
			}
			styler.SetLevel(skipLine, skipLevel + SC_FOLDLEVELBASE);
		}

		int lev = stateCurrent.indentCount + SC_FOLDLEVELBASE;
		if (!stateCurrent.Empty()) {
			if (stateCurrent.indentCount < stateNext.indentCount) {
				lev |= SC_FOLDLEVELHEADERFLAG;
			}
		}

		styler.SetLevel(lineCurrent, lev);
		stateCurrent = stateNext;
		lineCurrent = lineNext;
	}
}

}

LexerModule lmYAML(SCLEX_YAML, ColouriseYAMLDoc, "yaml", FoldYAMLDoc);