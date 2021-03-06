require 'rmagick'
include Magick
require 'sinatra'

class HelloWorldApp < Sinatra::Base
	get '/' do
		erb :form
	end
	post '/decode_image' do
		@image_filename = params[:image_file][:filename]
		image_file = params[:image_file][:tempfile]
		sectorSize = params[:sectorSize].to_i
		threshold = params[:threshold].to_i
		File.open("./public/#{@image_filename}", 'wb') do |f|
			f.write(image_file.read)
		end
		system("./c_stego/stego dec ./public/#{@image_filename} #{threshold} #{sectorSize} >./public/result.txt")
		@result_name = "result.txt"
		@result = File.read("./public/result.txt")
		erb :show_decoded
	end
	post '/encode_image' do
		@message_filename = params[:message_file][:filename]
		message_file = params[:message_file][:tempfile]
		@carrier_filename = params[:carrier_file][:filename]
		carrier_file = params[:carrier_file][:tempfile]
		sectorSize = params[:sectorSize].to_i
		File.open("./public/#{@carrier_filename}", 'wb') do |f|
			f.write(carrier_file.read)
		end
		File.open("./public/#{@message_filename}", 'wb') do |f|
			f.write(message_file.read)
		end
		if params[:rewind]=="on"
			system("./c_stego/stego enc ./public/#{@carrier_filename} ./public/#{@message_filename} ./public/enc#{@carrier_filename} #{sectorSize} rew >./public/log")
		else
			system("./c_stego/stego enc ./public/#{@carrier_filename} ./public/#{@message_filename} ./public/enc#{@carrier_filename} #{sectorSize} >./public/log")
		end
		system("./c_stego/stego diff ./public/#{@carrier_filename} ./public/enc#{@carrier_filename} ./public/diff#{@carrier_filename}")
		@threshold = File.open("./public/log")
		erb :show_image
	end
	post '/download' do
		filename = params[:file]
		send_file "./public/#{filename}", :filename => filename, :type => 'Application/octet-stream'
	end
end
