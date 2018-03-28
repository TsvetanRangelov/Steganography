require 'rmagick'
include Magick

if __FILE__ == $0
	im = Image.read(ARGV[0]).first
	im.write(ARGV[1])
end
