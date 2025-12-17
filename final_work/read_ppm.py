import PIL as pil
from PIL import Image
import sys
img = Image.open("images/" + sys.argv[1])
img.save("image.png")
img.show()
