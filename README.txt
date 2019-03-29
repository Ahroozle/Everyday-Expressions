# Everyday Expressions (Evex)

An experimental Thompson-esque regex system which attempts to implement functionalities common to backtracking (backreferences, recursion, etc.) efficiently into a DFA-based, non-backtracking format.

- - -

For those here in reference to P vs NP through 3-CNF-SAT, this does **NOT** solve it. The explanation as to why is in the <a href="#Afterword">afterword</a>.

- - -

## Features

- Special Characters
	- Escapes
		- [x] Exclusive Start (`\\A`)
			- Exclusively matches the beginning of the string, i.e. not line breaks before the first character
		- [x] Exclusive End (`\\z`)
			- Exclusively matches the end of the string, i.e. not line breaks after the last character
		- [x] Exclusive End Before Break (`\\Z`)
			- Matches the end and any number of line breaks immediately before it; i.e. if the string ends with line breaks, `\\Z` will match at those line breaks in addition to the end of the string.
		- [x] Word Boundaries (`\\b` and `\\B`)
			- Matches the boundaries of words, i.e. where one character satisfies "[A-Za-z0-9\_]" and the other satisfies "[^A-Za-z0-9\_]". `\\B` is the opposite, i.e. where both characters are word or non-word characters.
		- [x] Start of String or End of Last Match (`\\G`)
			- Only relevant in searches and not single matches. Matches the beginning of the string or the end of the last match before the first character. NOTE: Last match ends can be transferred between regexes via the regex constructor. This is allowed for the sake of convenient interaction between multiple different regexes.
		- [x] Literal Escape (`\\Q` and `\\E`)
			- All glyphs after `\\Q` are treated as literals until the end of the regex or `\\E` is found. Works inside and outside of character classes.
		- [ ] <s>Octal Escapes (`"\\0##"`)</s>
			- As an infinite number of backreferences can referenced, octal escapes have no syntax of their own, nor will I be giving them any.
	- Shorthands
		- [x] Lowercase (`\\l` and `\\L`)
			- Represent "[a-z]" and "[^a-z]"
		- [x] Uppercase (`\\u` and `\\U`)
			- Represent "[A-Z]" and "[^A-Z]"
		- [x] Digits (`\\d` and `\\D`)
			- Represent "[0-9]" and "[^0-9]"
		- [x] Word characters (`\\w` and `\\W`)
			- Represent "[A-Za-z0-9\_]" and "[^A-Za-z0-9\_]"
		- [x] All Whitespace (`\\s` and `\\S`)
			- Represent "[ \t\r\n\v\f]" and "[^ \t\r\n\v\f]"
		- [x] Horizontal Whitespace (`\\h` and `\\H`)
			- Represent "[ \t]" and "[^ \t]"
		- [x] Vertical Whitespace (`\\v` and `\\V`)
			- Represent "[\r\n\v\f]" and "[^\r\n\v\f]"
		- [x] Not Unix Line Break (`\\N`)
			- Represents "[^\n]"
		- [x] Not Line Break (`\\R`)
			- Represents "[.\r\n.\r\n\v\f]", i.e. it recognizes "\r\n" as a unit in addition to its individual pieces along with other vertical whitespace.
- Quantifiers
	- [x] Greedy Quantifiers (`"a?"`,`"a*"`,`"a+"`,`"a{N}"`,`"a{N,}"`,`"a{N,M}"`)
	- [x] Lazy Quantifiers (`"a??"`,`"a*?"`,`"a+?"`,`"a{N,}?"`,`"a{N,M}?"`)
	- [ ] <s>Possessive Quantifiers (`"a?+"`,`"a*+"`,`"a++"`,`"a{N,}+"`,`"a{N,M}+"`)</s>
		- Possessive quantifiers are not supported because Evex does not backtrack.
- Character Classes
	- [x] Regular Character Classes (`"[a]"`)
	- [x] Negated Character Classes (`"[^a]"`)
	- [x] Character Class Subtraction (`"[a-[b]]"`,`"[a-[b-[c]]]"`)
	- [x] Character Class Intersection (`"[a&&[b]]"`,`"[a&&[b&&[c]]]"`)
	- [x] Character Class Nesting (`"[a[b]]"`)
	- [ ] <s>POSIX Character Classes, Shorthands, and Character Equivalents (`"[[:alnum:]]"`,`"[[:s:]]"`, `"[[=e=]]"`)</s>
		- POSIX character classes, shorthands, and character equivalents are not supported.
	- Note: When building Character Classes with many operations in them, it is important to keep in mind: statements like "[a-[b]&&[c][d]]" are interpreted as the boolean expression "((a && !b) && c) || d", while ones akin to "[a-[b&&[c[d]]]]" are interpreted as "a && !(b && (c || d))". Nonbraced literals are considered unions after operations, i.e. "[a-[b]cd[e]]" is equivalent to "[a-[b][c][d][e]]".
- Groups
	- [x] Numbered Captures (`"(regex)"`)
	- [x] Named Captures (`"(?<name>regex)"`,`"(?'name'regex)"`)
	- [x] Non-Capturing Groups (`"(?:regex)"`)
	- [x] Branch Reset Groups (`"(?|(a)|(b)|(c))"`)
	- [x] Conditionals (`"(?(regex)then|else)"`,`"(?(regex)then)"`)
		- [x] Backreference Conditionals (`"(?(1)then|else)"`)
		- [x] Named Backreference Conditionals (`"(?(<name>)then|else)"`,`"(?('name')then|else)"`,`"(?({name})then|else)"`)
		- [x] Relative Backreference Conditionals (`"(?(-1)then|else)"`)
		- [x] Forward Reference Conditionals (`"(?(+1)then|else)"`)
- Backreferences
	- [x] Numbered Backreferences (`"(regex)\\1"`,`"(regex)\\k<1>"`,`"(regex)\\k'1'"`,`"(regex)\\k{1}"`)
	- [x] Relative Backreferences (`(regex)\\k<-1>`,`(regex)\\k'-1'`,`(regex)\\k{-1}`)
	- [x] Forward References (`"(\\2a|(b))+"`,`"(\\k<+1>a|(b))+"`,`"(\\k'+1'a|(b))+"`,`"(\\k{+1}a|(b))+"`)
	- [x] Nested Backreferences (`"(\\1a|(b))+"`)
	- [x] Named Backreferences (`"\\k<name>"`,`"\\k'name'"`,`"\\k{name}"`)
- Subroutines
	- [x] Numbered Subroutines (`"(regex)(?1)"`,`"(regex)\\g<1>"`,`"(regex)\\g'1'"`,`"(regex)\\g{1}"`)
	- [x] Relative Subroutines (`"(a)(?-1)"`,`"(regex)\\g<-1>"`,`"(regex)\\g'-1'"`,`"(regex)\\g{-1}"`)
	- [x] Forward Subroutines (`"(?1)(regex)"`,`"(?+1)(regex)"`,`"\\g<+1>(regex)"`,`"\\g'+1'(regex)"`,`"\\g{+1}(regex)"`)
	- [x] Nested Subroutines (`"((?1)?a|(b))"`)
	- [x] Named Subroutines (`"(?<name>)"`,`"(?'name')"`,`"\\g<name>"`,`"\\g'name'"`,`"\\g{name}"`)
	- [x] Subroutine Definitions (`"(?(DEFINE)regex)"`)
	- [x] Recursion (`"(?R)"`,`"(?0)"`,`"\\g<0>"`,`"\\g'0'"`,`"\\g{0}"`)
- Assertions
	- [x] Start-of-Line (`"^regex"`)
	- [x] End-of-Line (`"regex$"`)
	- [x] Lookahead (`"(?=regex)"`,`"(?!regex)"`)
	- [x] Lookbehind (`"(?<=regex)"`,`"(?<!regex)"`)
- Mode Modifiers
	- [x] Case Sensitivity (`"(?i)"`,`"(?c)"`)
	- [x] Single-Line/Multiline (`"(?s)"`,`"(?m)"`)
		- Changes the behavior of "^" and "$". Single-Line mode automatically interprets them as "\\A" and "\\z", while multiline is default functionality.
	- [x] No Auto-Capture (`"(?n)"`)
		- Only named captures are created; all other groups are automatically set to not capture.
	- [x] Unix Lines (`"(?d)"`)
		- Changes ".", "^", and "$" to only consider '\n' characters as line breaks. Shorthands and escapes are unaffected.
	- [ ] <s>Freespacing Mode (`"(?x)"`,`"(?t)"`,`"(?xx)"`)</s>
		- Freespacing is not supported, as I feel C/++ already gives you ways to organize a regex's components.
- Miscellaneous
	- [ ] <s> Comments (`"(?#comment)"`,`"(a)#comment"`) </s>
		- Inline and end-of-line comments are not supported, as I feel C/++ already gives you ways to comment on what a regex's parts all do.
- Differing Syntax
	- [x] Inline Code (`"(?{funcname})"`)
		- Unlike perl, you're unable to write code directly into the regex. Instead, when it gets to this node, it calls a function you've provided during construction, passing in state data for it to work with.
	- [x] Backreference and Subroutine Notation
		- In Evex, `\\k` exclusively means backreference, while `\\g` exclusively means subroutine. Recursion and subroutines, however are still denoted by their '(?#)' syntax in addition to their '\\\\g' syntax.
		- Certain group-esque notational variants for backreferences and subroutines ("(?P=#/name)", etc.) are also not supported.
- New Syntax
	- [x] Ligatures (`"[.ae.]"`,`"[.ae.abc]"`)
		- Surrounding any number of letters with two dots registers them as a ligature, meaning that the letters will be treated as a single unit. A single dot without a partner is assumed to be a literal and not indicative of a ligature.
	- [x] Capture Collections (`"(?@regex)"`,`"(?@<name>regex)"`,`"(?@'name'regex"`)
		- This signifies that a given capture group is a collection capture, meaning it behaves like .NET capture collections, i.e. stores every capture it has come across within a match.
	- [x] Manual Captures (`"(?$)"`,`(?$<name>)`,`(?$'name')`,`(?${name})`)
		- This signifies a capture which is not set during the match, but set by the user beforehand. Really just a convenience, reason for existing is explained in the <a href="#Afterword">afterword</a>.
	- [x] Lazy Groups Modifer (`"(?l)"`)
		- When the Lazy Groups modifier "(?l)" is set, all groups return at the shortest found match rather than the longest.
	- [x] Explicit DotAll Modifier (`"(?a)"`)
		- Specifically turns Dot-All mode on or off. "(?s)" and "(?m)" do not change the behavior of the dot in Evex.
- Unimplemented
	- [ ] Unicode support
		- Unicode support is intended to be implemented at some point in the future.
	- [ ] Replacement syntax and functionality
		- Replacement methods and syntax are intended to be implemented at some point in the future.

</br>

- - -

#### Motivation Behind Creation

This was an experiment to see if a method I'd thought up could achieve the powerful capability of backtracking regex implementations without the exponential-time downside. The result appears to achieve worst-case low polynomial time in relation to the text evaluated.

The line of logic this project was started to test is the following:
```
According to all sources I've seen on backreferences, a backreference simply looks for the result of the capture group it is bound to. i.e. a regex "(\w*)\1" would match all groupings of the same word twice with no spacing, e.g 'catcat', 'abcabc', etc.

Considering how we would normally construct a DFA, this type of behavior would be impossible; when construction is done, the DFA is static.

capture group      backreference
((a)->(b)->(c)->)->[???????????????]

There is quite literally no way a statically-constructed DFA can determine what a backreference could be, ever, which is especially problematic when you consider that backreferences are very commonly utilized nowadays.

However, if we take away the restriction that the DFA is only statically-constructed, a new option immediately pops from the aether:

capture group      backreference
((a)->(b)->(c)->)->[               ]
^

capture group      backreference
((a)->(b)->(c)->)->[(a)->          ]
  ^

capture group      backreference
((a)->(b)->(c)->)->[(a)->(b)->     ]
       ^

capture group      backreference
((a)->(b)->(c)->)->[(a)->(b)->(c)->]
            ^

... We can simply construct more nodes and add them into the graph dynamically!
```
As far as I can tell, the logic for this is sound for all regular backreferential inputs, and yields O(2N) time in reference to the constructed substring. Additionally, if you add the extra caveat that entry into the 'backreference graph' fails if the capture has not been reached or did not succeed, you get complete feature parity with backreferences in backtracking systems. Backrefs, Forward Refs, Nested Refs, the whole gamut.

In reality, of course, the system does not add to the graph dynamically. Instead, there is a special type of node which checks the final capture data itself. If the capture is in a fail state (i.e. it has not been reached yet, or has failed), the backreference node is not enterable. Otherwise, it does a simple check of the substring to see if it's been met, and if so it is then enterable.

As I continued to implement features of backtracking systems, one type of behavior seemed to occur more and more: recursion at the structural and functional levels. Over the course of development, the system has fully matured into something I'll just call a 'Recursive DFA', as I'm not familiar with any terminology that fits this type of automaton. As the name implies, it is a DFA that contains nodes which additionally contain other, smaller DFAs. How this new layout causes the system to deviate from Perl's regex behavior is mused upon in the Afterword.

</br>

- - -

<div id="Afterword"><h2>Afterword</h2></div>

This construction is merely the product of my speculation and curiosity into a line of reasoning which seemed like a simple answer to an odd problem; I by no means know the true bounds of how this system behaves. I also struggle to think of many good examples under which this method performs poorly, outside of some which are very easy to see:

1) If you took away the limiters I've written in, you could easily construct many different regexes which blow the C++ stack. Since the system is heavily recursive by nature, this was an Achilles' heel I was expecting it to have.
2) There exists an easy construction utilizing backreferences or subroutines which would completely tank performance. Regex "(a)(a)((?1)|(?2))" could be expanded along the lines of "(a)(a)...(a)((?1)|(?2)|...|(?N))" and then be fed a string such as "aa...ab", and I am reasonably sure that this will begin to take a very, *very* long time to fail as you continue to expand.

#### On the topic of P vs NP through 3-CNF-SAT:
The original site from which I got the news that 'backreferencing is NP-Complete' is [this one](https://swtch.com/~rsc/regexp/regexp1.html). After a long look at [the proof it provides](https://perl.plover.com/NPC/NPC-3SAT.html), I feel that I can confirm that this system does **NOT** solve P vs NP via P-Time 3-CNF-SAT, although it brings up an interesting discrepancy in the common interpretation of the proof.

Taking a look at the example given, it's quite clear what's going on:
```
string: 'xxx;x,x,x,x,'

regex: '(x?)(x?)(x?).*;
	(?:\1|\2|\3x),
	(?:\1|\2x|\3),
	(?:\1x|\2x|\3),
	(?:\1x|\2x|\3x),'
```
Almost immediately you may pick up that the backreferences are required. This makes complete sense; not having them is like having variables you can set, but can never get. Doing an operation with them would be impossible.

Recreating this with Evex, though, causes an interesting new difference to emerge:
```
string_1: 'ttt;t,t,t,t,' // true, true, true
string_2: 'ttf;t,t,t,t,' // true, true, false
.
.
.

regex: '(t?)(t?)(t?).*?;
	(?:\1|\2|\3t),
	(?:\1|\2t|\3),
	(?:\1t|\2t|\3),
	(?:\1t|\2t|\3t),'
```
While it is still very easily within the realm of possibility to solve 3-CNF-SAT, in Evex you must feed it each of the inputs yourself. Essentially, that which Perl does automatically must be done manually in Evex, due to its implementation.

For reference, a Perl equivalent of this regex would be:
```
regex: '(t?+)(t?+)(t?+).*?;
	(?:\1|\2|\3t)
	(?:\1|\2t|\3)
	(?:\1t|\2t|\3)
	(?:\1t|\2t|\3t)'
```
This additionally shows that this method is still serviceable in Perl. Whether this is indicative of Evex only encompassing a subset of the capabilities of Perl, or just that the overlaps between distinct regexes' sets of matches is narrower in Evex, is something I don't know.

The fact that the difference is so apparent between the Perl version and the Evex version reveals a crucial oversight in the interpretation of the proof: That it doesn't prove *backreferences* to be NP-Complete, just *Perl regexes.* Taking the logic behind how backreferences work into account doesn't bode well for such an interpretation either, considering I've shown how easy it is to implement backreferences in an efficient manner within the confines of a deterministic system. What's interesting, though, is that Perl's regexes themselves *still* aren't enough to explain their own NP-Completeness, as many (if not all with some caveats) Perl regexes have an Evex equivalent. Rather, Perl regexes' NP-Completeness comes from how they are *evaluated*, as opposed to any regex component you can point to. Going back to the example and its Evex equivalent, it's easy to see what's actually going on: it was constructed with the expectation that the three capture groups at the start would be evaluated many times, i.e. *backtracked* to, as Perl attempts to make the match work by any means necessary. The underlying reason Perl's regexes are NP-Complete is actually that Perl's regex matcher is made to solve a slightly different problem than the average regex matcher. Rather than answering the question 'Does this regex match this string?', Perl's regex matcher is actually answering the question 'Does this regex match this string, under *any* circumstance?', and the result of that small addition is a staggering difference in behavior and performance. Perl's matcher does the only known thing it can to solve the problem it's been given: *try as many permutations of the situation as it takes to get a match,* of which the runtime is exponential in nature. This is especially apparent when you consider that this question is a direct analogue to k-CNF-SAT's underlying question of 'Is there any circumstance (i.e. configuration of this set of booleans) under which all of these statements are satisfied?'

**tl;dr While backreferencing itself is essential for expressing NP-Complete problems within regexes, it's relatively easy to deduce that they themselves are not NP-Complete, just like how `bool A = value; if(A){}` doesn't take forever to run, but can be used to construct much more complex things which may. On closer inspection, one can deduce that the NP-Completeness of Perl regexes actually comes from how they are evaluated, rather than any attribute of their construction.**

</br>

As for the Evex-equivalent k-CNF-SAT solver, I'm personally irked by how much string wasting (or in the worse case, *regex* wasting) is involved with getting it to work. Thus, I've added a quick syntactic addition for the sake of convenience:
```
string: 't,t,t,t,'

regex: '(?$)(?$)(?$)
	(?:\1|\2|\3t)
	(?:\1|\2t|\3)
	(?:\1t|\2t|\3)
	(?:\1t|\2t|\3t)'

regexAutomaton.PresetCaptures({{1,'t'},{2,''},{3,'t'}}); // True, False, True
```
These new **Manual Captures** denoted by the `(?$)` groups don't capture anything. In fact, they aren't even parsed into the DFA. They solely exist to construct captures which are to be written to manually, cutting out a lot of tedious string constructions in the prior working example. Of course, k-CNF-SAT isn't their only use, although I can't think of any others off the top of my head.

</br>

- - -

I am not a career mathematician by any means. If any are inclined to think on what I've constructed, by all means feel free to. I'm genuinely curious what it is I've actually made; I doubt I can even call it a DFA or anything like one by this point.
<!-- Project started March 2nd, 2019. Initial commit made on March ###, 2019 -->
