import cv2 as cv
import numpy as np

def main():
    image = 'fang.JPG'
    
    # Load image
    src = cv.imread(cv.samples.findFile(image), cv.IMREAD_COLOR)
    # Make sure image is successfully loaded
    assert(src is not None)
    
    gray = cv.cvtColor(src, cv.COLOR_BGR2GRAY)
    gray = cv.medianBlur(gray, 5)
    
    print(gray.shape)
    rows = gray.shape[0]
    circles = cv.HoughCircles(gray, cv.HOUGH_GRADIENT, 1, 50, \
                              param1=150, param2=80, minRadius=10, maxRadius=60)
    
    if circles is not None:
        circles = np.uint16(np.around(circles))
        print(circles.shape)
        for i in circles[0, :]:
            center = (i[0], i[1])
            # mark circle center
            cv.circle(src, center, 1, (0, 100, 100), 3)
            # circle outline
            radius = i[2]
            cv.circle(src, center, radius, (255, 0, 255), 3)
    
    # Show image
    cv.namedWindow("detected circles", cv.WINDOW_NORMAL)
    src = cv.resize(src, (1280, 800))
    cv.imshow("detected circles", src)
    cv.waitKey(0)
    
    return 0
    
if __name__ == '__main__':
    main()
