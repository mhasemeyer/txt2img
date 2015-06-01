# Introduction #

txt2img is a tool that helps in tesseract training process by generating images and box files from a formatted text.


# Details #
  * Language: **C++**
  * GUI lib: **QT4.8**
  * Images format: **tiff**
  * Box format: **tesseract 3**, can be configured (see config.txt file in the project's root)

screenshot:
![http://txt2img.googlecode.com/files/txt2img0.2a.png](http://txt2img.googlecode.com/files/txt2img0.2a.png)


Characters statistics is used to highlight characters by their frequency of occurrence:

The application builds a histogram of characters (how many times a character appears in the document). Values for each characterd are normilized (divided by the number of occurrences of most frequent character). The color of a box depends on the value:

  * value < 0.25         - yellow
  * 0.25 < value  < 0.5  - cyan
  * 0.5  < value  < 0.75 - blue
  * 0.75 < value < 1     - green