require 'rmagick'
include Magick

if __FILE__ == $0
	im = Image.read("3plane.bmp").first
	im.write("nth.png")
end
