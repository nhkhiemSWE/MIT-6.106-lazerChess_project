def rotate_moves(move):
    result = ''
    for char in move:
        if (ord(char) >=97):
            result += chr(7 - ord(char) + 2*ord('a'))
        elif (ord(char) <= 57):
            result += char
        else:
            if (char == 'R'):
                result += 'L'
            elif (char == 'L'):
                result += 'R'
            else:
                result += char
    return result

moves = [ 'a1b2a6U', 'a0b1', 'a1Ra7L', 'a0a1', 'a1Ua7b7', 'a0U', 'a1La7a6', 'a1a2']
new_move = []
for move in moves:
    new_move.append(rotate_moves(move))

print(new_move)
