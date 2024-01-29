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


Mat performImageOperation(const Mat& inputImage, const std::string& operation, const std::string& PARAM) {
    Mat outputImage;

    if (operation == "resize") {
        resize(inputImage, outputImage, Size(300, 200));  // Adjust dimensions as needed
    } else if (operation == "rotate") {
        Point2f center(inputImage.cols / 2.0, inputImage.rows / 2.0);
        Mat rotationMatrix = getRotationMatrix2D(center, 90, 1.0);  // Adjust angle as needed
        warpAffine(inputImage, outputImage, rotationMatrix, inputImage.size());
    } else if (operation == "crop") {
        Rect cropRegion(50, 50, 200, 150);  // Adjust region as needed
        outputImage = inputImage(cropRegion);
    } else if (operation == "flip") {
        flip(inputImage, outputImage, 1);  // 0 for horizontal, 1 for vertical
    } else if (operation == "brightness") {
        inputImage.convertTo(outputImage, -1, 1, stoi(PARAM));  // Adjust brightness value as needed
    } else if (operation == "contrast") {
        inputImage.convertTo(outputImage, -1, 1.5, stoi(PARAM));  // Adjust contrast value as needed
    } else if (operation == "gamma") {
        LUT(inputImage, Mat_<float>::ones(1, 256) * 255.0, outputImage);
        pow(outputImage / 255.0, 1.8, outputImage);  // Adjust gamma value as needed
        outputImage *= 255;
    } else if (operation == "colorspace") {
        cvtColor(inputImage, outputImage, COLOR_BGR2GRAY);  // Adjust color space as needed
    } else if (operation == "gaussianblur") {
        GaussianBlur(inputImage, outputImage, Size(5, 5), stoi(PARAM));  // Adjust kernel size and sigma as needed
    } else if (operation == "boxblur") {
        blur(inputImage, outputImage, Size(5, 5));  // Adjust kernel size as needed
    } else if (operation == "sharpen") {
        Mat kernel = (Mat_<float>(3,3) << -1, -1, -1, -1, 9, -1, -1, -1, -1);
        filter2D(inputImage, outputImage, -1, kernel);
    } else {
        // Handle unsupported operation
        std::cerr << "Unsupported operation: " << operation << std::endl;
        return inputImage;
    }

    return outputImage;
}


struct image_metadata_t {
    std::string operation;
    std::string operation_PARAM;
    size_t image_size_bytes;
};

image_metadata_t parse_header(boost::asio::streambuf &buffer){

    std::string data_buff_str = std::string(boost::asio::buffer_cast<const char*>(buffer.data()));

    image_metadata_t meta_data;
    meta_data.operation = data_buff_str.substr(data_buff_str.find(",") + 1 , data_buff_str.find(";") - data_buff_str.find(",") -1 );
    meta_data.operation_PARAM = data_buff_str.substr(data_buff_str.find(";") + 1, data_buff_str.size() - 1);
    meta_data.image_size_bytes = std::stoi(data_buff_str.substr(0,data_buff_str.find(",")));

    return meta_data;
}

// this was created as shared ptr and we need later `this`
// therefore we need to inherit from enable_shared_from_this
class session : public std::enable_shared_from_this<session>
{
public:
    //session hold own socket
    session(tcp::socket socket)  
    : m_socket(std::move(socket)) { }
    
    void run() {
        wait_for_request();
    }
private:
    void wait_for_request() {
        // since we capture `this` in the callback, we need to call shared_from_this()
        auto self(shared_from_this());
        // and now call the lambda once data arrives
        // we read a string until the null termination character
        boost::asio::async_read_until(m_socket, m_buffer, "\0", 
        [this, self](boost::system::error_code ec, std::size_t /*length*/)
        {
            // if there was no error, everything went well and for this demo
            // we print the data to stdout and wait for the next request
            if (!ec)  {
                image_metadata_t header_data = parse_header(m_buffer);

                std::vector<std::uint8_t> buff(header_data.image_size_bytes);

                boost::asio::read(m_socket, boost::asio::buffer(buff), boost::asio::transfer_exactly(header_data.image_size_bytes), ignored_error);
                 
                if( ignored_error && ignored_error != boost::asio::error::eof ) {
                    cout << "SECOND receive failed: " << ignored_error.message() << endl;
                }
                else {
                    cv::Mat img(cv::imdecode(buff, cv::IMREAD_ANYCOLOR));

                    // Do Operations on the Image

                    if(!img.empty())
                    {
                        cout << header_data.operation << endl<< header_data.operation_PARAM<< endl;
                        img = performImageOperation(img, header_data.operation,header_data.operation_PARAM);

                        //Send Img Back to client
                        std::vector<int> param{cv::IMWRITE_JPEG_QUALITY, 95}; 
                        cv::imencode(".jpg", img, buff, param);
                        std:: string image_dimensions = "000Wx000H";
                        std:: string image_buff_bytes = std::to_string(buff.size());
                        std::string message_header = image_dimensions + "," +  image_buff_bytes;
                        std::cout << "sending measage header of " << std::to_string(message_header.length()) << " bytes...." << std::endl;
                        message_header.append(63 - message_header.length(), ' ');
                        message_header = message_header + '\0';
                        
                        m_socket.write_some(boost::asio::buffer(message_header), ignored_error);
                        m_socket.write_some(boost::asio::buffer(buff), ignored_error);
                    }
                }



                wait_for_request();
            } else {
                std::cout << "error: " << ec << std::endl;;
            }
        });
    }


private:

    boost::system::error_code ignored_error;
    tcp::socket m_socket;
    boost::asio::streambuf m_buffer;
    boost::asio::streambuf img_buffer;
};

class server
{
public:
    server(boost::asio::io_context& io_context, short port) 
    : m_acceptor(io_context, tcp::endpoint(tcp::v4(), port)) {
        do_accept();
    }
private:
    void do_accept() {
        // this is an async accept which means the lambda function is 
        // executed, when a client connects
        m_acceptor.async_accept([this](boost::system::error_code ec, tcp::socket socket) {
            if (!ec) {
                std::cout << "creating session on: " 
                    << socket.remote_endpoint().address().to_string() 
                    << ":" << socket.remote_endpoint().port() << '\n';
                // create a session 
                // the socket is passed to the lambda
                std::make_shared<session>(std::move(socket))->run();
            } else {
                std::cout << "error: " << ec.message() << std::endl;
            }
            // for multiple clients to connnect, wait for the next one by calling do_accept()
            do_accept();
        });
    }
private: 
    tcp::acceptor m_acceptor;
};


int main(int argc, char *argv[])
{

    if (argc< 2)
    {
        cout<<"Format ./server SERVER_PORT"<<endl;
        exit(-1);
    }

    std::string server_IP_PORT(argv[1]);
    int server_PORT = stoi(server_IP_PORT.substr(0, server_IP_PORT.size()));

    try
    {
        // Create the context
        boost::asio::io_context io_context;
        //Start the server
        server s(io_context, server_PORT);
        //run the Server
        io_context.run();

        return 0;

    }
    catch (std::exception& e)
    {
        std::cerr << e.what() << std::endl;
    }

    return 0; 
}
