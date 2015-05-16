#pragma once

enum class ELexerToken
{
	// Value types
	Integer,
	Float,
	String,
	Identifier,
	Keyword, /**< Keyword. */

	Dot, /**< '.'. */
	Colon, /**< ':'. */
	Semicolon, /**< ';'. */
	Comma,
	Pound, /**< '#'. */
	At, /**< '@'. */

	OpenParenthesis, /**< '('. */
	CloseParenthesis,
	OpenBrace, /**< '{'. */
	CloseBrace,
	OpenBracket, /**< '['. */
	CloseBracket,

	ExclamationMark,
	QuestionMark,
	Ampersand, /**< '&'. */
	Pipe, /**< '|'. */
	LessThan, /**< '<'. */
	GreaterThan, /**< '>'. */

	Percent, /**< '%'. */
	Slash, /**< '/'. */
	BackSlash, /**< '\'. */
	Assign, /**< '='. */
	Asterisk, /**< '*'. */
	Plus,
	Dash, /**< '-'. */

	// Compound instructions
	DoubleColon, /**< '::'. */
	DoublePipe, /**< '||'. */
	DoubleAmpersand, /**< '&&'. */

	Increment, /**< '++'. */
	Decrement, /**< '--' */
	AddAssign, /**< '+=' */
	SubtractAssign, /**< '-=' */
	MultiplyAssign, /**< '*=' */
	DivideAssign, /**< '/=' */
	ModuloAssign, /**< '%=' */

	Equal, /**< '=='. */
	NotEqual, /**< '!='. */
	LessOrEqual, /**< '<='. */
	GreaterOrEqual, /**< '>='. */

	Comment,
	NewLine,
	End,
	Invalid
};