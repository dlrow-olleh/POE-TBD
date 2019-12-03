import cv2
    # - cv2 is a computer vision library
import numpy as np
    #- numpy library which will now be referred to as np

cam = cv2.VideoCapture(0)
    # - set variable "cam" which captures camera vision

while(True):
    # - Capture frame-by-frame
    ret, frame = cam.read()
        # - cam.read() should return a bool

    rgb = cv2.cvtColor(frame, cv2.COLOR_BGR2BGRA)
        # - gets an image in black and white

    # - Displays the resulting frame
    cv2.imshow('frame', rgb)
    if cv2.waitKey(1) & 0xFF ==ord('p'):
        print('picture taken')
        out = cv2.imwrite('capture.jpg', frame)
        break

# - Releases the capture when done
cam.release()
print('cam closed')

# - Closes all created windows
cv2.destroyWindow('frame')

while(True):
# - sets jpg as img, the 0 input signifies grayscale
    img = cv2.imread('capture.jpg', 0)
        #displays img on the image window

    edges = cv2.Canny(img, 60,90) #50,100
        # Canny finds the edges of an image
            # calls picture, min, and max threshold
    out = cv2.imwrite('edges.jpg', edges)
    cv2.imshow('image',img)
    cv2.imshow('edges', edges)
    print('image displayed')

    # - If opened` for one millisecond and q is pressed then it closes
    if cv2.waitKey(1) & 0xFF == ord('q'):
        cv2.destroyAllWindows()
        print('ALL CLOSED')
        break
