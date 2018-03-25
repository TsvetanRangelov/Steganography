require 'rmagick'
include Magick
require 'sinatra'

class HelloWorldApp < Sinatra::Base
	get '/' do
		erb :form
	end
	post '/save_image' do
		@message_filename = params[:message_file][:filename]
		message_file = params[:message_file][:tempfile]
		@carrier_filename = params[:carrier_file][:filename]
		carrier_file = params[:carrier_file][:tempfile]
		File.open("./public/#{@carrier_filename}", 'wb') do |f|
			f.write(carrier_file.read)
		end
		File.open("./public/#{@message_filename}", 'wb') do |f|
			f.write(message_file.read)
		end
		encoded_ok = system ( "./c_stego/s.sh #{@carrier_filename} #{@message_filename} dump.txt")
		print encoded_ok
		system ( "./c_stego/diff.sh #{@carrier_filename} #{@carrier_filename}2 #{@carrier_filename}3")
		erb :show_image
	end
	post '/download' do
		filename = params[:file]
		send_file "./public/#{filename}", :filename => filename, :type => 'Application/octet-stream'
	end
end
