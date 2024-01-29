#include <iostream>
#include <boost/array.hpp>
#include <boost/asio.hpp>
#include <boost/thread/thread.hpp>
#include "opencv2/highgui/highgui.hpp"
#include "opencv2/imgproc/imgproc.hpp"
#include <opencv2/core/core.hpp> 
#include <string>
#include <vector>

using boost::asio::ip::tcp;
using namespace std;
using namespace cv;

bool flag = false;    

struct image_metadata_t {
    int width;
    int height;
    size_t image_size_bytes;
};

image_metadata_t parse_header(boost::asio::streambuf &buffer){

    std::string data_buff_str = std::string(boost::asio::buffer_cast<const char*>(buffer.data()));
    cout << data_buff_str << endl;

    int width_pos = data_buff_str.find("W"); 
    int x_pos = data_buff_str.find("x"); 
    int height_pos = data_buff_str.find("H"); 
    int comma_pos = data_buff_str.find(","); 

    std::cout << "data_buff_str.substr(x_pos+1,height_pos) " << data_buff_str.substr(x_pos+1,height_pos) <<std::endl;

    image_metadata_t meta_data;
    meta_data.width = std::stoi(data_buff_str.substr(0,width_pos));
    meta_data.height = std::stoi(data_buff_str.substr(x_pos+1,height_pos));
    meta_data.image_size_bytes = std::stoi(data_buff_str.substr(data_buff_str.find(",") + 1));

    return meta_data;
}

cv::Mat retrieve_data(std::string path){

    std::string image_path = path;
    cv::Mat image;
    image = imread(image_path, cv::IMREAD_COLOR);
    if(! image.data ) {
        std::cout << "Could not open or find the image" << std::endl;
    }
    return image;
}


int main(int argc, char *argv[])
{
    if(strcmp(argv[1], "-h") == 0)
    {

        cout << "gaussianblur 'amount'"<<endl;
        cout << "resize"<<endl;
        cout << "crop"<<endl;
        cout << "flip"<<endl;
        cout << "brightness 'amount'"<<endl;
        cout << "contrast 'amount'"<<endl;
        cout << "sharpen"<<endl;
        cout << "colorspace "<<endl;
        cout << "boxblur"<<endl;

        exit(1);

    }
    cout << argv[1]<< endl;
    cout << strcmp(argv[1], "-h")<< endl;

    if (argc< 4)
    {
        cout<<"Format ./client SERVER_IP:SERVER_PORT IMAGE_ADDRESS OPERATION \n FOR OPERATIONS TRY -h"<<endl;
        exit(-1);
    }

    std::string server_IP_PORT(argv[1]);
    std::string server_IP = server_IP_PORT.substr(0, server_IP_PORT.find(":"));
    int server_PORT = stoi(server_IP_PORT.substr(server_IP_PORT.find(":") + 1, server_IP_PORT.size() - 1));

    std::string imgADDR(argv[2]);
    std::string operation(argv[3]);
    std::string operation_PARAM(argv[4]);

    try{
        boost::asio::io_service io_service;
        tcp::endpoint end_point(boost::asio::ip::address::from_string(server_IP), server_PORT);
        tcp::socket socket(io_service);
        socket.connect(end_point);
        boost::system::error_code ignored_error;
        boost::asio::streambuf receive_buffer;

        cv::Mat frame = retrieve_data(imgADDR);
        std::vector<std::uint8_t> buff;
        std::vector<int> param{cv::IMWRITE_JPEG_QUALITY, 95}; 
        cv::imencode(".jpg", frame, buff, param);

        std:: string image_buff_bytes = std::to_string(buff.size());
        
        std::string message_header = image_buff_bytes + "," + operation + ";" + operation_PARAM;
        std::cout << "sending measage header of " << std::to_string(message_header.length()) << " bytes...." << std::endl;
        message_header.append(63 - message_header.length(), ' ');
        message_header = message_header + '\0';

        socket.write_some(boost::asio::buffer(message_header), ignored_error);

        socket.write_some(boost::asio::buffer(buff), ignored_error);

        size_t header_size = 64;
        boost::asio::read(socket, receive_buffer, boost::asio::transfer_exactly(header_size), ignored_error);

        if ( ignored_error && ignored_error != boost::asio::error::eof ) {
            cout << "first receive failed: " << ignored_error.message() << endl;
        } else {
            image_metadata_t header_data = parse_header(receive_buffer);

            // Now we retrieve the frame itself
            std::cout << "Now asing for image bytes of size " << std::to_string(header_data.image_size_bytes) << std::endl;
            
            //boost::asio::streambuf second_receive_buffer;
            std::vector<std::uint8_t> buff(header_data.image_size_bytes);
            boost::asio::read(socket, boost::asio::buffer(buff), boost::asio::transfer_exactly(header_data.image_size_bytes), ignored_error);
            socket.close();

            if( ignored_error && ignored_error != boost::asio::error::eof ) {
                cout << "SECOND receive failed: " << ignored_error.message() << endl;
            }
            else {
                std::cout << "image bytes  = " << std::to_string(header_data.image_size_bytes) << std::endl;

                cv::Mat img(cv::imdecode(buff, cv::IMREAD_ANYCOLOR));

                //imshow("Original", frame);
                imshow("Server Sent", img);
                waitKey(0);
                imwrite("./Image/ServerOutput.jpg", img);
           	}   
        }
    } catch (std::exception& e) {
        std::cerr << e.what() << std::endl;
    }

    return 0; 
}




















    // return 0;
    //     flag = true;
    
    // boost::thread thrd(&servershow);
    // try
    // {
    //     boost::asio::io_service io_service;
    //     tcp::acceptor acceptor(io_service, tcp::endpoint(tcp::v4(), 3200));
        
    //     while(1) {
    //         tcp::socket socket(io_service);
    //         acceptor.accept(socket);
    //         boost::system::error_code ignored_error;

    //         // Retrieve the frame to be sent
    //         cv::Mat frame = retrieve_data();
    //         std::vector<std::uint8_t> buff;
    //         std::vector<int> param{cv::IMWRITE_JPEG_QUALITY, 95}; 
    //         cv::imencode(".jpg", frame, buff, param);

    //         // Send the header message
    //         std:: string image_dimensions = "6016Wx3384H";
    //         std:: string image_buff_bytes = std::to_string(buff.size());
    //         std::string message_header = image_dimensions + "," +  image_buff_bytes;
    //         std::cout << "sending measage header of " << std::to_string(message_header.length()) << " bytes...." << std::endl;
    //         message_header.append(63 - message_header.length(), ' ');
    //         message_header = message_header + '\0';

    //         socket.write_some(boost::asio::buffer(message_header), ignored_error);

    //         socket.write_some(boost::asio::buffer(buff), ignored_error);

    //         flag = true;
    //     }
    // }
    // catch (std::exception& e)
    // {
    //     std::cerr << e.what() << std::endl;
    // }
    // thrd.join();
