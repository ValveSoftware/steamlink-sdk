# `Source/core/layout`

## Overflow rects and scroll offsets

PaintLayerScrollableArea uses a "scroll origin" to conceptually represent the distance between
the top-left corner of the box'es content rect and the top-left corner of its overflow rect
when the box is scrolled to the logical beginning of its content (.e.g. all the way to the left for
LTR, all the way to the right for RTL).  For left-to-right and top-to-bottom flows, the scroll
origin is zero, i.e., the top/left of the overflow rect is at the same position as the top/left of
the box'es content rect when scrolled to the beginning of flow.  For right-to-left and bottom-to-top
flows, the overflow rect extends to the top/left of the client rect.

The default calculation for scroll origin is the distance between the top-left corner of the content
rect and the top-left corner of the overflow rect.  To illustrate, here is a box with left overflow and
no vertical scrollbar:

                             content
                               rect
                           |<-------->|
                    scroll
                    origin
                |<-------->|
                 _____________________
                |          |          |
                |          |          |
                |          |          |
direction:rtl   |          |   box    |
                |          |          |
                |          |          |
                |__________|__________|
                
                      overflow rect
                |<--------------------->|


However, if the box has a scrollbar for the orthogonal direction (e.g., a vertical scrollbar
in a direction:rtl block), the size of the scrollbar must be added to the scroll origin calculation.
Here are two examples -- note that it doesn't matter whether the vertical scrollbar is placed on
the right or left of the box (the vertical scrollbar is the |/| part):

                               content
                                 rect
                             |<-------->|
                    scroll
                    origin
                |<---------->|
                 _______________________
                |          |/|          |
                |          |/|          |
                |          |/|          |
direction:rtl   |          |/|   box    |
                |          |/|          |
                |          |/|          |
                |__________|/|__________|

                      overflow rect
                |<--------------------->|
                
                
                
                                 content
                                   rect
                               |<-------->|
                      scroll
                      origin
                  |<---------->|
                     _______________________
                    |          |          |/|
                    |          |          |/|
                    |          |          |/|
writing-mode:       |          |          |/|
  vertical-rl       |          |          |/|
                    |          |          |/|
                    |          |          |/|
                    |          |          |/|
                    |__________|__________|/|
                    
                          overflow rect
                    |<--------------------->|
