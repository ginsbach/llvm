import Data.Char
import Data.List
import Debug.Trace
import qualified Data.Map as Map
import qualified Data.Maybe as Maybe

import GeneratedParser
import IDLProcessing

data CharType = Digit Char | Whitespace Char | Normal Char | Special Char

skipChars::String->[CharType]
skipChars []        = []
skipChars ('\n':xs) = classifyChars xs
skipChars (_:xs)    = skipChars xs

classifyChars::String -> [CharType]
classifyChars [] = []
classifyChars ('#':xs)      = skipChars xs
classifyChars (x:xs)
    | x `elem` "(){}[]=.+-,@&|" = Special x:classifyChars xs
    | isDigit x                 = Digit x:classifyChars xs
    | isSpace x                 = Whitespace x:classifyChars xs
    | otherwise                 = Normal x:classifyChars xs

tokenize::[CharType]->[SyntaxType]
tokenize [] = []
tokenize (Special '.':Special '.':xs) = PLiteral "..":tokenize xs
tokenize (Special '-':Special '>':xs) = PLiteral "->":tokenize xs
tokenize (Special a:xs)               = PLiteral [a] :tokenize xs
tokenize (Whitespace _:xs)            = tokenize xs
tokenize (Digit a:Digit b:xs)         = case tokenize (Digit b:xs) of (PNumber n:ns) -> PNumber (a:n):ns
tokenize (Digit a:xs)                 = PNumber [a]:tokenize xs
tokenize (Normal a:Digit  b:xs)       = case tokenize (Digit  b:xs) of (PNumber n:ns) -> PLiteral (a:n):ns
tokenize (Normal a:Normal b:xs)       = case tokenize (Normal b:xs) of (PLiteral n:ns) -> PLiteral (a:n):ns
tokenize (Normal a:xs)                = PLiteral [a]:tokenize xs

parse::[SyntaxType]->[[SyntaxType]]->[[SyntaxType]]
parse xs     [] = error ("Syntax error before "++show xs)
parse []     ys = ys
parse (x:xs) ys = parse xs [z|y<-ys,z<-match (x:y)]

unbox::[[SyntaxType]]->[SyntaxType]
unbox [x] = x

filterExports::Map.Map String SyntaxType->[SyntaxType]->[SyntaxType]
filterExports map ((PNode "specification" [PLiteral n, c]):(PNode "export" []):xs) = ((PNode "specification" [PLiteral n, (Maybe.fromJust $ Map.lookup n map)]):(filterExports map xs))
filterExports map ((PNode "specification" x):xs) = filterExports map xs
filterExports map [] = []

collectSpecifications::[SyntaxType]->Map.Map String SyntaxType
collectSpecifications ((PNode "specification" [PLiteral n, c]):xs) = Map.insert n c $ collectSpecifications xs
collectSpecifications (x:xs) = collectSpecifications xs
collectSpecifications [] = Map.empty

removeInvisibles::[SyntaxType]->[SyntaxType]
removeInvisibles ((PNode "#" [cont]):xs)     = removeInvisibles [cont] ++ removeInvisibles xs
removeInvisibles ((PNode ('@':str) cont):xs) = removeInvisibles cont ++ removeInvisibles xs
removeInvisibles ((PNode      str  cont):xs) = (PNode str $ removeInvisibles cont):removeInvisibles xs
removeInvisibles (x:xs)                      = x:removeInvisibles xs
removeInvisibles []                          = []

prettyprint::SyntaxType->String
prettyprint (PLiteral s)     = show s
prettyprint (PNumber  n)     = show $ (read::(String->Int)) n
prettyprint (PNode str cont) = ("("++intercalate ", " (show str:map prettyprint cont)++",)")

main = do
    contents <- getContents
    let parsed  = parse (tokenize $ classifyChars contents) [[PNode "#" []]]
    let cleaned = init $ removeInvisibles $ unbox $ parsed
    let specifs = collectSpecifications cleaned
    let simpler = Map.mapMaybe (simplify specifs) specifs
    let exports = filterExports simpler cleaned
    putStrLn $ "("++intercalate ", " (map prettyprint exports)++",)"
