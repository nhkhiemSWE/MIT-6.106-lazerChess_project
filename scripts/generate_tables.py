ARR_WIDTH = 10
ARR_SIZE = (ARR_WIDTH * ARR_WIDTH)

BOARD_WIDTH = 8

FIL_ORIGIN = ((ARR_WIDTH - BOARD_WIDTH) / 2)
RNK_ORIGIN = ((ARR_WIDTH - BOARD_WIDTH) / 2)

FIL_SHIFT = 4
FIL_MASK = 0xF
RNK_SHIFT = 0
RNK_MASK = 0xF

def print_table(tbl,tbl_name,type_str):
    """
        tbl: list to print, should have size ARR_SIZE
    """
    print(f"static {type_str} {tbl_name}[ARR_SIZE] = {{",end=' ')
    for i in range(ARR_SIZE-1):
        print(tbl[i],end=', ')

        if i%ARR_WIDTH == ARR_WIDTH-1:
            print()

    print(f"{tbl[ARR_SIZE-1]} }};")

    return

qi_table = []
fil_table = []
rkn_table = []

for sq in range(ARR_SIZE):
  f = ((sq >> FIL_SHIFT) & FIL_MASK) - FIL_ORIGIN;
  r = ((sq >> RNK_SHIFT) & RNK_MASK) - RNK_ORIGIN;

  rows_from_center = 2 * r - 7;
  files_from_center = 2 * f - 7;

  qi_value = 98 - (rows_from_center * rows_from_center + files_from_center * files_from_center)
  qi_value = max(qi_value,0)
  qi_table.append(qi_value);

  fil_table.append(f)
  rkn_table.append(r)

print_table(qi_table,'qi_table','uint8_t')
print_table(fil_table,'fil_table','int')
print_table(rkn_table,'rkn_table','int') 